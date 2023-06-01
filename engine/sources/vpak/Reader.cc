#include <vull/vpak/Reader.hh>

#include <vull/container/Array.hh>
#include <vull/container/Vector.hh>
#include <vull/maths/Common.hh>
#include <vull/platform/File.hh>
#include <vull/platform/FileStream.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Optional.hh>
#include <vull/support/PerfectHasher.hh>
#include <vull/support/Result.hh>
#include <vull/support/Span.hh>
#include <vull/support/Stream.hh>
#include <vull/support/StreamError.hh>
#include <vull/support/String.hh>
#include <vull/support/StringView.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/support/Utility.hh>
#include <vull/vpak/PackFile.hh>

#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <zstd.h>

namespace vull::vpak {
namespace {

struct Context {
    ZSTD_DCtx *dctx;
    uint8_t *buffer;

    Context() {
        dctx = ZSTD_createDCtx();
        buffer = new uint8_t[ZSTD_DStreamOutSize()];
    }
    Context(ZSTD_DCtx *ctx, uint8_t *buf) : dctx(ctx), buffer(buf) {}
    Context(const Context &) = delete;
    Context(Context &&other)
        : dctx(vull::exchange(other.dctx, nullptr)), buffer(vull::exchange(other.buffer, nullptr)) {}
    ~Context() {
        if (dctx == nullptr) {
            return;
        }
        delete[] buffer;
        ZSTD_freeDCtx(dctx);
    }

    Context &operator=(const Context &) = delete;
    Context &operator=(Context &&) = delete;
};
VULL_GLOBAL(thread_local Vector<Context> s_contexts);

} // namespace

ReadStream::ReadStream(LargeSpan<uint8_t> data, size_t first_block) : m_data(data), m_block_start(first_block) {
    if (s_contexts.empty()) {
        s_contexts.emplace();
    }

    auto &context = s_contexts.last();
    m_dctx = vull::exchange(context.dctx, nullptr);
    m_buffer = vull::exchange(context.buffer, nullptr);
    s_contexts.pop();

    ZSTD_DCtx_reset(m_dctx, ZSTD_reset_session_only);
    m_compressed_size = ZSTD_findFrameCompressedSize(m_data.byte_offset(m_block_start), m_data.size() - m_block_start);
}

ReadStream::~ReadStream() {
    s_contexts.emplace(m_dctx, m_buffer);
}

Result<size_t, StreamError> ReadStream::read(Span<void> data) {
    // TODO: Is this loop needed?
    for (uint32_t bytes_read = 0; bytes_read < data.size();) {
        ZSTD_inBuffer input{
            .src = m_data.byte_offset(m_block_start + m_offset),
            .size = vull::min(m_compressed_size, ZSTD_CStreamOutSize() - m_offset),
        };
        VULL_ENSURE(input.size > 0);

        // Use a temporary buffer to avoid reading back from uncached vulkan memory.
        ZSTD_outBuffer output{
            .dst = m_buffer,
            // TODO: Always read ZSTD_DStreamOutSize and cache data for next call to read().
            .size = vull::min(data.size() - bytes_read, static_cast<uint32_t>(ZSTD_DStreamOutSize())),
        };
        size_t rc = ZSTD_decompressStream(m_dctx, &output, &input);
        VULL_ENSURE(ZSTD_isError(rc) == 0);
        memcpy(data.byte_offset(bytes_read), m_buffer, output.pos);
        bytes_read += output.pos;

        m_compressed_size -= input.pos;
        m_offset += input.pos;
        if (m_offset >= ZSTD_CStreamOutSize()) {
            size_t next_offset = m_block_start + ZSTD_CStreamOutSize();
            m_block_start = (static_cast<uint64_t>(m_data[next_offset]) << 56u) |
                            (static_cast<uint64_t>(m_data[next_offset + 1]) << 48u) |
                            (static_cast<uint64_t>(m_data[next_offset + 2]) << 40u) |
                            (static_cast<uint64_t>(m_data[next_offset + 3]) << 32u) |
                            (static_cast<uint64_t>(m_data[next_offset + 4]) << 24u) |
                            (static_cast<uint64_t>(m_data[next_offset + 5]) << 16u) |
                            (static_cast<uint64_t>(m_data[next_offset + 6]) << 8u) |
                            (static_cast<uint64_t>(m_data[next_offset + 7]) << 0u);
            m_compressed_size =
                ZSTD_findFrameCompressedSize(m_data.byte_offset(m_block_start), m_data.size() - m_block_start);
            m_offset = 0;
        }
    }
    return static_cast<size_t>(data.size());
}

Reader::Reader(File &&file) {
    auto stream = file.create_stream();
    VULL_ENSURE(VULL_EXPECT(stream.read_byte()) == 'V');
    VULL_ENSURE(VULL_EXPECT(stream.read_byte()) == 'P');
    VULL_ENSURE(VULL_EXPECT(stream.read_byte()) == 'A');
    VULL_ENSURE(VULL_EXPECT(stream.read_byte()) == 'K');

    const auto entry_count = VULL_EXPECT(stream.read_be<uint32_t>());
    const auto entry_table_offset = VULL_EXPECT(stream.read_be<uint64_t>());
    VULL_EXPECT(stream.seek(entry_table_offset, SeekMode::Set));

    Vector<int32_t> seeds;
    seeds.ensure_capacity(entry_count);
    for (uint32_t i = 0; i < entry_count; i++) {
        const auto seed = VULL_EXPECT(stream.read_be<uint32_t>());
        seeds.push(static_cast<int32_t>(seed));
    }
    m_phf = {vull::move(seeds)};

    m_entries.ensure_size(entry_count);
    for (auto &entry : m_entries) {
        entry.type = static_cast<EntryType>(VULL_EXPECT(stream.read_byte()));
        entry.name = VULL_EXPECT(stream.read_string());
        entry.size = VULL_EXPECT(stream.read_varint<uint32_t>());
        entry.first_block = VULL_EXPECT(stream.read_varint<uint64_t>());
    }

    // mmapping will keep the fd open even when the File is destructed.
    struct stat stat {};
    fstat(file.fd(), &stat);
    const auto file_size = static_cast<size_t>(stat.st_size);
    m_data = {static_cast<uint8_t *>(mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, file.fd(), 0)), file_size};
}

Reader::~Reader() {
    munmap(m_data.data(), m_data.size());
}

bool Reader::exists(StringView name) const {
    return m_entries[m_phf.hash(name)].name.view() == name;
}

UniquePtr<ReadStream> Reader::open(StringView name) const {
    const auto &entry = m_entries[m_phf.hash(name)];
    return entry.name.view() == name ? vull::make_unique<ReadStream>(m_data, entry.first_block)
                                     : UniquePtr<ReadStream>();
}

Optional<Entry> Reader::stat(StringView name) const {
    const auto &entry = m_entries[m_phf.hash(name)];
    return entry.name.view() == name ? entry : Optional<Entry>();
}

} // namespace vull::vpak
