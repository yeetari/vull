#include <vull/vpak/PackReader.hh>

#include <vull/core/Log.hh>
#include <vull/maths/Common.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Optional.hh>
#include <vull/support/Span.hh>
#include <vull/vpak/PackFile.hh>

#include <string.h>
#include <sys/mman.h>
#include <zstd.h>

namespace vull {

PackReader::PackReader(FILE *file) : m_dctx(ZSTD_createDCtx()), m_buffer(new uint8_t[ZSTD_DStreamOutSize()]) {
    fseek(file, 0, SEEK_END);
    m_size = static_cast<size_t>(ftell(file));
    m_data = static_cast<uint8_t *>(mmap(nullptr, m_size, PROT_READ, MAP_PRIVATE, fileno(file), 0));
    fseek(file, 0, SEEK_SET);
}

PackReader::~PackReader() {
    munmap(m_data, m_size);
    delete[] m_buffer;
    ZSTD_freeDCtx(m_dctx);
}

void PackReader::read_header() {
    VULL_ENSURE(read_byte() == 'V');
    VULL_ENSURE(read_byte() == 'P');
    VULL_ENSURE(read_byte() == 'A');
    VULL_ENSURE(read_byte() == 'K');
    m_compressed_size = 0;
}

Optional<PackEntry> PackReader::read_entry() {
    m_compressed = false;
    m_head += m_compressed_size;
    if (m_head >= m_size) {
        return {};
    }
    uint8_t type_byte = read_byte();
    PackEntry entry{
        .size = static_cast<uint32_t>(read_byte() << 24u) | static_cast<uint32_t>(read_byte() << 16u) |
                static_cast<uint32_t>(read_byte() << 8u) | static_cast<uint32_t>(read_byte() << 0u),
        .type = static_cast<PackEntryType>(type_byte & 0x7fu),
    };
    m_compressed_size = entry.size;
    if ((m_compressed = (((type_byte >> 7u) & 1u) == 1u))) {
        ZSTD_DCtx_reset(m_dctx, ZSTD_reset_session_only);
        m_compressed_size = ZSTD_findFrameCompressedSize(m_data + m_head, m_size - m_head);
        VULL_ENSURE(ZSTD_isError(m_compressed_size) == 0);
    }
    return entry;
}

void PackReader::read(Span<void> data) {
    if (!m_compressed) {
        memcpy(data.data(), m_data + m_head, data.size());
        m_head += data.size();
        m_compressed_size -= data.size();
        return;
    }
    ZSTD_inBuffer input{
        .src = m_data + m_head,
        .size = m_compressed_size,
    };
    for (uint32_t bytes_written = 0; bytes_written < data.size();) {
        // Use a temporary buffer to avoid reading back from uncached vulkan memory.
        ZSTD_outBuffer output{
            .dst = m_buffer,
            .size = vull::min(data.size(), static_cast<uint32_t>(ZSTD_DStreamOutSize())),
        };
        size_t ret = ZSTD_decompressStream(m_dctx, &output, &input);
        if (ZSTD_isError(ret) == 1) {
            vull::error("[vpak] ZSTD error: '{}'", ZSTD_getErrorName(ret));
            VULL_ENSURE_NOT_REACHED();
        }
        memcpy(static_cast<uint8_t *>(data.data()) + bytes_written, m_buffer, output.pos);
        bytes_written += output.pos;
    }
    m_head += input.pos;
    m_compressed_size -= input.pos;
}

uint8_t PackReader::read_byte() {
    uint8_t byte;
    read({&byte, 1});
    return byte;
}

uint32_t PackReader::read_varint() {
    uint32_t value = 0;
    for (uint32_t byte_count = 0; byte_count < sizeof(uint32_t); byte_count++) {
        uint8_t byte = read_byte();
        value |= static_cast<uint32_t>(byte & 0x7fu) << (byte_count * 7u);
        if ((byte & 0x80u) == 0u) {
            break;
        }
    }
    return value;
}

} // namespace vull
