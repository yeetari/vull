#include <vull/vpak/Reader.hh>

#include <vull/maths/Common.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Optional.hh>
#include <vull/support/PerfectHasher.hh>
#include <vull/support/Span.hh>
#include <vull/support/String.hh>
#include <vull/support/StringView.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>
#include <vull/vpak/PackFile.hh>

#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zstd.h>

namespace vull::vpak {
namespace {

VULL_GLOBAL(thread_local ZSTD_DCtx *s_dctx = nullptr);
VULL_GLOBAL(thread_local uint8_t *s_dbuffer = nullptr);
VULL_GLOBAL(thread_local struct ThreadDestructor { // NOLINT
    ~ThreadDestructor() {
        delete[] s_dbuffer;
        ZSTD_freeDCtx(s_dctx);
    }
} s_destructor);

} // namespace

ReadStream::ReadStream(LargeSpan<uint8_t> data, off64_t first_block)
    : m_data(data), m_block_start(static_cast<size_t>(first_block)) {
    static_cast<void>(s_destructor);
    if (s_dctx == nullptr) {
        VULL_ASSERT(s_dbuffer == nullptr);
        s_dctx = ZSTD_createDCtx();
        s_dbuffer = new uint8_t[ZSTD_DStreamOutSize()];
    }
    ZSTD_DCtx_reset(s_dctx, ZSTD_reset_session_only);
    m_compressed_size = ZSTD_findFrameCompressedSize(m_data.byte_offset(m_block_start), m_data.size() - m_block_start);
}

void ReadStream::read(Span<void> data) {
    // TODO: Is this loop needed?
    for (uint32_t bytes_read = 0; bytes_read < data.size();) {
        ZSTD_inBuffer input{
            .src = m_data.byte_offset(m_block_start + m_offset),
            .size = vull::min(m_compressed_size, ZSTD_CStreamOutSize() - m_offset),
        };
        VULL_ENSURE(input.size > 0);

        // Use a temporary buffer to avoid reading back from uncached vulkan memory.
        ZSTD_outBuffer output{
            .dst = s_dbuffer,
            // TODO: Always read ZSTD_DStreamOutSize and cache data for next call to read().
            .size = vull::min(data.size() - bytes_read, static_cast<uint32_t>(ZSTD_DStreamOutSize())),
        };
        size_t rc = ZSTD_decompressStream(s_dctx, &output, &input);
        VULL_ENSURE(ZSTD_isError(rc) == 0);
        memcpy(data.byte_offset(bytes_read), s_dbuffer, output.pos);
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
}

uint8_t ReadStream::read_byte() {
    // TODO: Shorter path.
    uint8_t byte;
    read({&byte, 1});
    return byte;
}

uint64_t ReadStream::read_varint() {
    uint64_t value = 0;
    for (uint64_t byte_count = 0; byte_count < sizeof(uint64_t); byte_count++) {
        uint8_t byte = read_byte();
        value |= static_cast<uint64_t>(byte & 0x7fu) << (byte_count * 7u);
        if ((byte & 0x80u) == 0u) {
            break;
        }
    }
    return value;
}

Reader::Reader(const String &path) : m_fd(::open(path.data(), O_RDONLY)) {
    VULL_ENSURE(m_fd >= 0);

    struct stat stat {};
    fstat(m_fd, &stat);

    const auto file_size = static_cast<size_t>(stat.st_size);
    m_data = {static_cast<uint8_t *>(mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, m_fd, 0)), file_size};
    close(m_fd);

    VULL_ENSURE(m_data[0] == 'V');
    VULL_ENSURE(m_data[1] == 'P');
    VULL_ENSURE(m_data[2] == 'A');
    VULL_ENSURE(m_data[3] == 'K');

    uint32_t entry_count = (static_cast<uint32_t>(m_data[4]) << 24u) | (static_cast<uint32_t>(m_data[5]) << 16u) |
                           (static_cast<uint32_t>(m_data[6]) << 8u) | (static_cast<uint32_t>(m_data[7]) << 0u);
    uint64_t entry_table_offset =
        (static_cast<uint64_t>(m_data[8]) << 56u) | (static_cast<uint64_t>(m_data[9]) << 48u) |
        (static_cast<uint64_t>(m_data[10]) << 40u) | (static_cast<uint64_t>(m_data[11]) << 32u) |
        (static_cast<uint64_t>(m_data[12]) << 24u) | (static_cast<uint64_t>(m_data[13]) << 16u) |
        (static_cast<uint64_t>(m_data[14]) << 8u) | (static_cast<uint64_t>(m_data[15]) << 0u);

    Vector<int32_t> seeds;
    seeds.ensure_capacity(entry_count);
    for (uint64_t offset = entry_table_offset; offset < entry_table_offset + static_cast<uint64_t>(entry_count) * 4;
         offset += 4) {
        uint32_t seed =
            (static_cast<uint32_t>(m_data[offset + 0]) << 24u) | (static_cast<uint32_t>(m_data[offset + 1]) << 16u) |
            (static_cast<uint32_t>(m_data[offset + 2]) << 8u) | (static_cast<uint32_t>(m_data[offset + 3]) << 0u);
        seeds.push(static_cast<int32_t>(seed));
    }
    m_phf = {vull::move(seeds)};

    m_entries.ensure_size(entry_count);
    for (uint64_t offset = entry_table_offset + entry_count * sizeof(int32_t); auto &entry : m_entries) {
        entry.type = static_cast<EntryType>(m_data[offset++]);
        entry.name = String::copy_raw(&m_data.as<char>()[offset + 1], m_data[offset]);
        offset += m_data[offset] + 1;

        // TODO(stream-api)
        for (size_t byte_count = 0; byte_count < sizeof(uint32_t); byte_count++) {
            uint8_t byte = m_data[offset++];
            entry.size |= static_cast<uint32_t>(byte & 0x7fu) << (byte_count * 7u);
            if ((byte & 0x80u) == 0u) {
                break;
            }
        }
        for (size_t byte_count = 0; byte_count < sizeof(off64_t); byte_count++) {
            uint8_t byte = m_data[offset++];
            entry.first_block |= static_cast<off64_t>(byte & 0x7fu) << (byte_count * 7u);
            if ((byte & 0x80u) == 0u) {
                break;
            }
        }
    }
}

Reader::~Reader() {
    munmap(m_data.data(), m_data.size());
}

bool Reader::exists(StringView name) const {
    return m_entries[m_phf.hash(name)].name.view() == name;
}

Optional<ReadStream> Reader::open(StringView name) const {
    const auto &entry = m_entries[m_phf.hash(name)];
    return entry.name.view() == name ? ReadStream(m_data, entry.first_block) : Optional<ReadStream>();
}

Optional<Entry> Reader::stat(StringView name) const {
    const auto &entry = m_entries[m_phf.hash(name)];
    return entry.name.view() == name ? entry : Optional<Entry>();
}

} // namespace vull::vpak
