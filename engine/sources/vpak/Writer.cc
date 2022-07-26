#include <vull/vpak/Writer.hh>

#include <vull/core/Log.hh>
#include <vull/maths/Common.hh>
#include <vull/support/Algorithm.hh>
#include <vull/support/Array.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Optional.hh>
#include <vull/support/PerfectHasher.hh>
#include <vull/support/ScopeGuard.hh>
#include <vull/support/Span.hh>
#include <vull/support/String.hh>
#include <vull/support/StringView.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>
#include <vull/vpak/PackFile.hh>

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <zstd.h>

namespace vull::vpak {
namespace {

// constexpr version of ZSTD_CStreamOutSize.
constexpr off64_t k_zstd_block_size = ZSTD_COMPRESSBOUND(ZSTD_BLOCKSIZE_MAX) + 3 + 4;
constexpr off64_t k_block_size = k_zstd_block_size + off64_t(sizeof(off64_t));
VULL_GLOBAL(thread_local ZSTD_CCtx *s_cctx = nullptr);
VULL_GLOBAL(thread_local uint8_t *s_cbuffer = nullptr);
VULL_GLOBAL(thread_local struct ThreadDestructor { // NOLINT
    ~ThreadDestructor() {
        delete[] s_cbuffer;
        ZSTD_freeCCtx(s_cctx);
    }
} s_destructor);

} // namespace

WriteStream::WriteStream(Writer &writer, Entry &entry) : m_writer(writer), m_entry(entry) {
    static_cast<void>(s_destructor);
    if (s_cctx == nullptr) {
        VULL_ASSERT(s_cbuffer == nullptr);
        s_cctx = ZSTD_createCCtx();
        s_cbuffer = new uint8_t[k_zstd_block_size];
        ZSTD_CCtx_setParameter(s_cctx, ZSTD_c_contentSizeFlag, 0);
        ZSTD_CCtx_setParameter(s_cctx, ZSTD_c_checksumFlag, 1);
    }
    ZSTD_CCtx_reset(s_cctx, ZSTD_reset_session_only);
    switch (writer.m_clevel) {
    case CompressionLevel::Fast:
        ZSTD_CCtx_setParameter(s_cctx, ZSTD_c_compressionLevel, ZSTD_minCLevel());
        break;
    case CompressionLevel::Normal:
        ZSTD_CCtx_setParameter(s_cctx, ZSTD_c_compressionLevel, 19);
        break;
    case CompressionLevel::Ultra:
        ZSTD_CCtx_setParameter(s_cctx, ZSTD_c_compressionLevel, ZSTD_maxCLevel());
        break;
    }
}

WriteStream::~WriteStream() {
    // Ensure that finish() has been called.
    VULL_ASSERT(m_block_link_offset == 0 && m_compress_head == 0);
}

void WriteStream::flush_block() {
    VULL_ASSERT(m_compress_head == k_zstd_block_size);
    off64_t block = m_writer.allocate(k_block_size);
    m_entry.first_block = m_entry.first_block != 0 ? m_entry.first_block : block;
    if (m_block_link_offset != 0) {
        m_writer.write_raw(static_cast<uint64_t>(block), m_block_link_offset);
    }
    m_writer.write_raw({s_cbuffer, k_zstd_block_size}, block);
    m_block_link_offset = block + k_zstd_block_size;
    m_compressed_size += k_zstd_block_size;
    m_compress_head = 0;
}

float WriteStream::finish() {
    ScopeGuard guard([this] {
        // Clear state to signal that finish() has been called.
        m_block_link_offset = 0;
        m_compress_head = 0;
    });

    size_t remaining = 0;
    do {
        ZSTD_inBuffer input{};
        ZSTD_outBuffer output{
            .dst = s_cbuffer + m_compress_head,
            .size = static_cast<size_t>(k_zstd_block_size - m_compress_head),
        };
        if (output.size == 0) {
            flush_block();
        }
        remaining = ZSTD_compressStream2(s_cctx, &output, &input, ZSTD_e_end);
        m_compress_head += static_cast<off64_t>(output.pos);
    } while (remaining != 0);

    // Flush any remaining buffered data.
    if (m_compress_head > 0) {
        off64_t block = m_writer.allocate(m_compress_head);
        m_entry.first_block = m_entry.first_block != 0 ? m_entry.first_block : block;
        if (m_block_link_offset != 0) {
            m_writer.write_raw(static_cast<uint64_t>(block), m_block_link_offset);
        }
        m_writer.write_raw({s_cbuffer, static_cast<uint32_t>(m_compress_head)}, block);
        m_compressed_size += static_cast<uint32_t>(m_compress_head);
    }
    return static_cast<float>(m_compressed_size) / static_cast<float>(m_entry.size) * 100.0f;
}

// TODO: Better input buffering.
void WriteStream::write(Span<const void> data) {
    m_entry.size += data.size();
    for (size_t bytes_written = 0; bytes_written < data.size();) {
        ZSTD_inBuffer input{
            .src = data.byte_offset(static_cast<uint32_t>(bytes_written)),
            .size = vull::min(data.size() - bytes_written, ZSTD_CStreamInSize()),
        };
        do {
            ZSTD_outBuffer output{
                .dst = s_cbuffer + m_compress_head,
                .size = static_cast<size_t>(k_zstd_block_size - m_compress_head),
            };
            if (output.size == 0) {
                // Flush full write buffer to new block.
                flush_block();
                continue;
            }
            ZSTD_compressStream2(s_cctx, &output, &input, ZSTD_e_continue);
            m_compress_head += static_cast<off64_t>(output.pos);
        } while (input.pos != input.size);
        bytes_written += input.size;
    }
}

void WriteStream::write_byte(uint8_t byte) {
    m_entry.size++;
    ZSTD_inBuffer input{
        .src = &byte,
        .size = sizeof(uint8_t),
    };
    if (m_compress_head >= k_zstd_block_size) {
        // Flush full write buffer to new block.
        flush_block();
    }
    ZSTD_outBuffer output{
        .dst = s_cbuffer + m_compress_head,
        .size = static_cast<size_t>(k_zstd_block_size - m_compress_head),
    };
    ZSTD_compressStream2(s_cctx, &output, &input, ZSTD_e_continue);
    m_compress_head += static_cast<off64_t>(output.pos);
}

void WriteStream::write_varint(uint64_t value) {
    while (value >= 128) {
        write_byte((value & 0x7fu) | 0x80u);
        value >>= 7u;
    }
    write_byte(value & 0x7fu);
}

Writer::Writer(const String &path, CompressionLevel clevel) : m_fd(creat(path.data(), 0666)), m_clevel(clevel) {
    VULL_ENSURE(m_fd >= 0);
    pthread_mutex_init(&m_mutex, nullptr);

    Array magic_bytes{'V', 'P', 'A', 'K'};
    write_raw(magic_bytes.span());

    // Reserve space for entry_count and entry_table_offset.
    lseek(m_fd, sizeof(uint32_t) + sizeof(uint64_t), SEEK_CUR);
}

Writer::~Writer() {
    close(m_fd);
    pthread_mutex_destroy(&m_mutex);
}

uint64_t Writer::finish() {
    write_entry_table();
    return static_cast<uint64_t>(lseek64(m_fd, 0, SEEK_CUR));
}

WriteStream Writer::start_entry(String name, EntryType type) {
    pthread_mutex_lock(&m_mutex);
    auto &entry = *m_entries.emplace(new Entry{
        .name = vull::move(name),
        .type = type,
    });
    pthread_mutex_unlock(&m_mutex);
    return {*this, entry};
}

off64_t Writer::allocate(off64_t size) {
    pthread_mutex_lock(&m_mutex);
    off64_t offset = lseek64(m_fd, size, SEEK_CUR) - size;
    pthread_mutex_unlock(&m_mutex);
    return offset;
}

void Writer::write_entry_table() {
    vull::debug("[vpak] Writing entry table ({} entries)", m_entries.size());

    // Fill in entry_count and entry_table_offset at the start of the file.
    const auto entry_table_offset = static_cast<uint64_t>(lseek64(m_fd, 0, SEEK_CUR));
    write_raw(m_entries.size(), sizeof(uint32_t));
    write_raw(entry_table_offset, sizeof(uint32_t) * 2);

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
        write_raw(static_cast<uint32_t>(seed));
    }
    for (const auto &entry : m_entries) {
        // See PackFile.hh for the definition of an entry header.
        write_raw(static_cast<uint8_t>(entry->type));
        write_raw(static_cast<uint8_t>(entry->name.length()));
        write_raw(entry->name.view().as<const void, uint32_t>());

        // TODO(stream-api): Duplicated with write_varint.
        auto size = entry->size;
        while (size >= 128) {
            write_raw(static_cast<uint8_t>((size & 0x7fu) | 0x80u));
            size >>= 7u;
        }
        write_raw(static_cast<uint8_t>(size & 0x7fu));

        auto first_block = static_cast<uint64_t>(entry->first_block);
        while (first_block >= 128) {
            write_raw(static_cast<uint8_t>((first_block & 0x7fu) | 0x80u));
            first_block >>= 7u;
        }
        write_raw(static_cast<uint8_t>(first_block & 0x7fu));
    }
}

void Writer::write_raw(Span<const void> data, Optional<off64_t> offset) const {
    // TODO: Propagate any errors.
    if (offset) {
        VULL_ENSURE(pwrite(m_fd, data.data(), data.size(), *offset) == data.size());
    } else {
        VULL_ENSURE(write(m_fd, data.data(), data.size()) == data.size());
    }
}

void Writer::write_raw(uint8_t value, Optional<off64_t> offset) const {
    write_raw({&value, 1}, offset);
}

void Writer::write_raw(uint32_t value, Optional<off64_t> offset) const {
    Array<uint8_t, 4> bytes{
        static_cast<uint8_t>((value >> 24u) & 0xffu),
        static_cast<uint8_t>((value >> 16u) & 0xffu),
        static_cast<uint8_t>((value >> 8u) & 0xffu),
        static_cast<uint8_t>((value >> 0u) & 0xffu),
    };
    write_raw(bytes.span(), offset);
}

void Writer::write_raw(uint64_t value, Optional<off64_t> offset) const {
    Array<uint8_t, 8> bytes{
        static_cast<uint8_t>((value >> 56u) & 0xffu), static_cast<uint8_t>((value >> 48u) & 0xffu),
        static_cast<uint8_t>((value >> 40u) & 0xffu), static_cast<uint8_t>((value >> 32u) & 0xffu),
        static_cast<uint8_t>((value >> 24u) & 0xffu), static_cast<uint8_t>((value >> 16u) & 0xffu),
        static_cast<uint8_t>((value >> 8u) & 0xffu),  static_cast<uint8_t>((value >> 0u) & 0xffu),
    };
    write_raw(bytes.span(), offset);
}

} // namespace vull::vpak
