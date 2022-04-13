#include <vull/vpak/PackWriter.hh>

#include <vull/maths/Common.hh>
#include <vull/support/Array.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Optional.hh>
#include <vull/support/ScopeGuard.hh>
#include <vull/support/Span.hh>
#include <vull/support/Utility.hh>
#include <vull/vpak/PackFile.hh>

#include <stdio.h>
#include <zstd.h>

namespace vull {

PackWriter::PackWriter(FILE *file)
    : m_file(file), m_cctx(ZSTD_createCCtx()), m_buffer(new uint8_t[ZSTD_CStreamOutSize()]) {
    ZSTD_CCtx_setParameter(m_cctx, ZSTD_c_compressionLevel, ZSTD_maxCLevel());
    ZSTD_CCtx_setParameter(m_cctx, ZSTD_c_contentSizeFlag, 0);
    ZSTD_CCtx_setParameter(m_cctx, ZSTD_c_checksumFlag, 1);
}

PackWriter::~PackWriter() {
    delete[] m_buffer;
    ZSTD_freeCCtx(m_cctx);
}

void PackWriter::write_header() {
    VULL_ASSERT(!m_compress_head);
    Array magic_bytes{'V', 'P', 'A', 'K'};
    write(magic_bytes.span());
}

void PackWriter::start_entry(PackEntryType type, bool compressed) {
    VULL_ASSERT(!m_compress_head);
    ZSTD_CCtx_reset(m_cctx, ZSTD_reset_session_only);
    write_byte(static_cast<uint8_t>(type));

    // Reserve 4 bytes for the entry size.
    m_size_seek_back = ftello(m_file);
    fseeko(m_file, sizeof(uint32_t), SEEK_CUR);

    m_compressed_size = 0;
    m_entry_size = 0;
    if (compressed) {
        m_compress_head.emplace(0ul);
    }
}

float PackWriter::end_entry() {
    ScopeGuard size_filler([this] {
        // Go back and fill in the entry size. We take a copy of m_entry_size as it will be increased by write_byte.
        const auto entry_size = m_entry_size;
        fseeko(m_file, m_size_seek_back, SEEK_SET);
        write_byte((entry_size >> 24u) & 0xffu);
        write_byte((entry_size >> 16u) & 0xffu);
        write_byte((entry_size >> 8u) & 0xffu);
        write_byte((entry_size >> 0u) & 0xffu);
        fseeko(m_file, 0, SEEK_END);
    });

    if (!m_compress_head) {
        return 1.0f;
    }

    size_t remaining = 0;
    do {
        ZSTD_inBuffer input{};
        ZSTD_outBuffer output{
            .dst = m_buffer + *m_compress_head,
            .size = ZSTD_CStreamOutSize() - *m_compress_head,
        };
        if (output.size == 0) {
            // Flush full write buffer.
            m_compressed_size += fwrite(m_buffer, 1, exchange(*m_compress_head, 0ul), m_file);
        }
        remaining = ZSTD_compressStream2(m_cctx, &output, &input, ZSTD_e_end);
        *m_compress_head += output.pos;
    } while (remaining != 0);

    // Flush any remaining compressed data.
    m_compressed_size += fwrite(m_buffer, 1, *m_compress_head, m_file);
    m_compress_head.clear();
    return static_cast<float>(m_compressed_size) / static_cast<float>(m_entry_size);
}

void PackWriter::write(Span<const void> data) {
    m_entry_size += data.size();
    if (!m_compress_head) {
        // Write out any uncompressed data directly.
        fwrite(data.data(), 1, data.size(), m_file);
        return;
    }
    for (size_t bytes_written = 0; bytes_written < data.size();) {
        ZSTD_inBuffer input{
            .src = static_cast<const uint8_t *>(data.data()) + bytes_written,
            .size = min(data.size() - bytes_written, ZSTD_CStreamInSize()),
        };
        do {
            ZSTD_outBuffer output{
                .dst = m_buffer + *m_compress_head,
                .size = ZSTD_CStreamOutSize() - *m_compress_head,
            };
            if (output.size == 0) {
                // Flush full write buffer.
                m_compressed_size += fwrite(m_buffer, 1, exchange(*m_compress_head, 0ul), m_file);
            }
            ZSTD_compressStream2(m_cctx, &output, &input, ZSTD_e_continue);
            *m_compress_head += output.pos;
        } while (input.pos != input.size);
        bytes_written += input.size;
    }
}

void PackWriter::write_byte(uint8_t byte) {
    write({&byte, 1});
}

void PackWriter::write_varint(uint32_t value) {
    while (value >= 128) {
        write_byte((value & 0x7fu) | 0x80u);
        value >>= 7u;
    }
    write_byte(value & 0x7fu);
}

} // namespace vull
