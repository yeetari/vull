#include <vull/vpak/Writer.hh>

#include <vull/core/Log.hh>
#include <vull/maths/Common.hh>
#include <vull/support/Algorithm.hh>
#include <vull/support/Array.hh>
#include <vull/support/Assert.hh>
#include <vull/support/PerfectHasher.hh>
#include <vull/support/Result.hh>
#include <vull/support/ScopedLock.hh>
#include <vull/support/Span.hh>
#include <vull/support/Stream.hh>
#include <vull/support/StreamError.hh>
#include <vull/support/String.hh>
#include <vull/support/StringView.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>
#include <vull/vpak/PackFile.hh>

#include <zstd.h>

namespace vull::vpak {
namespace {

// constexpr version of ZSTD_CStreamOutSize.
constexpr size_t k_zstd_block_size = ZSTD_COMPRESSBOUND(ZSTD_BLOCKSIZE_MAX) + 3 + 4;
constexpr size_t k_block_size = k_zstd_block_size + sizeof(size_t);

struct Context {
    ZSTD_CCtx *cctx;
    uint8_t *buffer;

    Context() {
        cctx = ZSTD_createCCtx();
        buffer = new uint8_t[k_zstd_block_size];
        ZSTD_CCtx_setParameter(cctx, ZSTD_c_contentSizeFlag, 0);
        ZSTD_CCtx_setParameter(cctx, ZSTD_c_checksumFlag, 1);
    }
    Context(ZSTD_CCtx *ctx, uint8_t *buf) : cctx(ctx), buffer(buf) {}
    Context(const Context &) = delete;
    Context(Context &&other)
        : cctx(vull::exchange(other.cctx, nullptr)), buffer(vull::exchange(other.buffer, nullptr)) {}
    ~Context() {
        if (cctx == nullptr) {
            return;
        }
        delete[] buffer;
        ZSTD_freeCCtx(cctx);
    }

    Context &operator=(const Context &) = delete;
    Context &operator=(Context &&) = delete;
};
VULL_GLOBAL(thread_local Vector<Context> s_contexts);

} // namespace

WriteStream::WriteStream(Writer &writer, UniquePtr<Stream> &&stream, Entry &entry)
    : m_writer(writer), m_stream(vull::move(stream)), m_entry(entry) {
    if (s_contexts.empty()) {
        s_contexts.emplace();
    }

    auto &context = s_contexts.last();
    m_cctx = vull::exchange(context.cctx, nullptr);
    m_buffer = vull::exchange(context.buffer, nullptr);
    s_contexts.pop();

    ZSTD_CCtx_reset(m_cctx, ZSTD_reset_session_only);
    switch (writer.m_clevel) {
    case CompressionLevel::Fast:
        ZSTD_CCtx_setParameter(m_cctx, ZSTD_c_compressionLevel, ZSTD_minCLevel());
        break;
    case CompressionLevel::Normal:
        ZSTD_CCtx_setParameter(m_cctx, ZSTD_c_compressionLevel, 19);
        break;
    case CompressionLevel::Ultra:
        ZSTD_CCtx_setParameter(m_cctx, ZSTD_c_compressionLevel, ZSTD_maxCLevel());
        break;
    }
}

WriteStream::~WriteStream() {
    // Ensure that all data has been flushed.
    VULL_ASSERT(m_compress_head == 0);
    s_contexts.emplace(m_cctx, m_buffer);
}

Result<void, StreamError> WriteStream::flush_block() {
    const bool last = m_compress_head != k_zstd_block_size;
    size_t block_offset = VULL_TRY(m_writer.allocate(last ? m_compress_head : k_block_size));
    if (m_entry.first_block == 0) {
        m_entry.first_block = block_offset;
    }
    if (m_block_link_offset != 0) {
        VULL_TRY(m_stream->seek(m_block_link_offset, SeekMode::Set));
        VULL_TRY(m_stream->write_be(block_offset));
    }
    VULL_TRY(m_stream->seek(block_offset, SeekMode::Set));
    VULL_TRY(m_stream->write({m_buffer, m_compress_head}));
    m_block_link_offset = block_offset + m_compress_head;
    m_compressed_size += m_compress_head;
    m_compress_head = 0;
    return {};
}

float WriteStream::finish() {
    size_t remaining = 0;
    do {
        ZSTD_inBuffer input{};
        ZSTD_outBuffer output{
            .dst = m_buffer + m_compress_head,
            .size = static_cast<size_t>(k_zstd_block_size - m_compress_head),
        };
        if (output.size == 0) {
            VULL_EXPECT(flush_block());
        }
        remaining = ZSTD_compressStream2(m_cctx, &output, &input, ZSTD_e_end);
        m_compress_head += output.pos;
    } while (remaining != 0);

    // Flush any remaining buffered data.
    if (m_compress_head > 0) {
        VULL_EXPECT(flush_block());
    }
    return static_cast<float>(m_compressed_size) / static_cast<float>(m_entry.size) * 100.0f;
}

// TODO: Better input buffering.
Result<void, StreamError> WriteStream::write(Span<const void> data) {
    m_entry.size += data.size();
    for (size_t bytes_written = 0; bytes_written < data.size();) {
        ZSTD_inBuffer input{
            .src = data.byte_offset(static_cast<uint32_t>(bytes_written)),
            .size = vull::min(data.size() - bytes_written, ZSTD_CStreamInSize()),
        };
        do {
            ZSTD_outBuffer output{
                .dst = m_buffer + m_compress_head,
                .size = static_cast<size_t>(k_zstd_block_size - m_compress_head),
            };
            if (output.size == 0) {
                // Flush full write buffer to new block.
                VULL_EXPECT(flush_block());
                continue;
            }
            ZSTD_compressStream2(m_cctx, &output, &input, ZSTD_e_continue);
            m_compress_head += output.pos;
        } while (input.pos != input.size);
        bytes_written += input.size;
    }
    return {};
}

Result<void, StreamError> WriteStream::write_byte(uint8_t byte) {
    m_entry.size++;
    ZSTD_inBuffer input{
        .src = &byte,
        .size = sizeof(uint8_t),
    };
    if (m_compress_head >= k_zstd_block_size) {
        // Flush full write buffer to new block.
        VULL_EXPECT(flush_block());
    }
    ZSTD_outBuffer output{
        .dst = m_buffer + m_compress_head,
        .size = static_cast<size_t>(k_zstd_block_size - m_compress_head),
    };
    ZSTD_compressStream2(m_cctx, &output, &input, ZSTD_e_continue);
    m_compress_head += output.pos;
    return {};
}

Writer::Writer(UniquePtr<Stream> &&stream, CompressionLevel clevel) : m_stream(vull::move(stream)), m_clevel(clevel) {
    VULL_EXPECT(read_existing());
    VULL_EXPECT(m_stream->seek(0, SeekMode::Set));

    Array magic_bytes{'V', 'P', 'A', 'K'};
    VULL_EXPECT(m_stream->write(magic_bytes.span()));

    // Reserve space for entry_count and entry_table_offset (using dummy data instead of SeekMode::Add so seeking to the
    // end works properly).
    Array<uint8_t, 12> dummy;
    VULL_EXPECT(m_stream->write(dummy.span()));
    VULL_EXPECT(m_stream->seek(0, SeekMode::End));
}

uint64_t Writer::finish() {
    write_entry_table();
    return VULL_EXPECT(m_stream->seek(0, SeekMode::Add));
}

WriteStream Writer::start_entry(String name, EntryType type) {
    auto new_entry = vull::make_unique<Entry>(Entry{
        .name = name,
        .type = type,
    });
    ScopedLock lock(m_mutex);
    // TODO: Use a hash map.
    for (auto &entry : m_entries) {
        if (entry->name == name) {
            vull::warn("[vpak] Overwriting {}", name);
            entry = vull::move(new_entry);
            return {*this, m_stream->clone_unique(), *entry};
        }
    }
    auto &entry = *m_entries.emplace(vull::move(new_entry));
    return {*this, m_stream->clone_unique(), entry};
}

Result<uint64_t, StreamError> Writer::allocate(size_t size) {
    ScopedLock lock(m_mutex);
    return VULL_TRY(m_stream->seek(size, SeekMode::Add)) - size;
}

Result<void, StreamError> Writer::read_existing() {
    if (VULL_TRY(m_stream->seek(0, SeekMode::End)) == 0) {
        // File empty, creating a new vpak.
        return {};
    }

    // Seek past magic number.
    // TODO: Should check the magic number.
    VULL_TRY(m_stream->seek(4, SeekMode::Set));

    // TODO: Duplicated with Reader.
    const auto entry_count = VULL_TRY(m_stream->read_be<uint32_t>());
    const auto entry_table_offset = VULL_TRY(m_stream->read_be<uint64_t>());
    VULL_TRY(m_stream->seek(entry_table_offset + entry_count * sizeof(uint32_t), SeekMode::Set));

    m_entries.ensure_size(entry_count);
    for (auto &entry : m_entries) {
        entry = vull::make_unique<Entry>();
        entry->type = static_cast<EntryType>(VULL_TRY(m_stream->read_byte()));
        entry->name = VULL_TRY(m_stream->read_string());
        entry->size = VULL_TRY(m_stream->read_varint<uint32_t>());
        entry->first_block = VULL_TRY(m_stream->read_varint<uint64_t>());
    }
    return {};
}

void Writer::write_entry_table() {
    vull::debug("[vpak] Writing entry table ({} entries)", m_entries.size());

    // Fill in entry_count and entry_table_offset at the start of the file.
    const auto entry_table_offset = VULL_EXPECT(m_stream->seek(0, SeekMode::Add));
    VULL_EXPECT(m_stream->seek(sizeof(uint32_t), SeekMode::Set));
    VULL_EXPECT(m_stream->write_be(m_entries.size()));
    VULL_EXPECT(m_stream->write_be(entry_table_offset));
    VULL_EXPECT(m_stream->seek(entry_table_offset, SeekMode::Set));

    Vector<StringView> keys;
    keys.ensure_capacity(m_entries.size());
    for (const auto &entry : m_entries) {
        keys.push(entry->name);
    }

    PerfectHasher phf;
    phf.build(keys);
    vull::sort(m_entries, [&](const auto &lhs, const auto &rhs) {
        return phf.hash(lhs->name) > phf.hash(rhs->name);
    });

    for (int32_t seed : phf.seeds()) {
        VULL_EXPECT(m_stream->write_be(static_cast<uint32_t>(seed)));
    }
    for (const auto &entry : m_entries) {
        // See PackFile.hh for the definition of an entry header.
        VULL_EXPECT(m_stream->write_byte(static_cast<uint8_t>(entry->type)));
        VULL_EXPECT(m_stream->write_string(entry->name));
        VULL_EXPECT(m_stream->write_varint(entry->size));
        VULL_EXPECT(m_stream->write_varint(entry->first_block));
    }
}

} // namespace vull::vpak
