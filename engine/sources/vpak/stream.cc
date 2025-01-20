#include <vull/vpak/stream.hh>

#include <vull/container/vector.hh>
#include <vull/maths/common.hh>
#include <vull/support/assert.hh>
#include <vull/support/result.hh>
#include <vull/support/span.hh>
#include <vull/support/stream.hh>
#include <vull/support/string.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>
#include <vull/vpak/defs.hh>
#include <vull/vpak/writer.hh>

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <zstd.h>

namespace vull::vpak {
namespace {

constexpr size_t k_input_block_size = 1u << 17u;
constexpr size_t k_output_block_size = ZSTD_COMPRESSBOUND(1u << 17u) + 3;

struct ReadContext {
    ZSTD_DCtx *dctx;
    uint8_t *in_buffer;
    uint8_t *out_buffer;

    ReadContext();
    ReadContext(ZSTD_DCtx *ctx, uint8_t *in_buf, uint8_t *out_buf)
        : dctx(ctx), in_buffer(in_buf), out_buffer(out_buf) {}
    ReadContext(const ReadContext &) = delete;
    ReadContext(ReadContext &&other)
        : dctx(vull::exchange(other.dctx, nullptr)), in_buffer(vull::exchange(other.in_buffer, nullptr)),
          out_buffer(vull::exchange(other.out_buffer, nullptr)) {}
    ~ReadContext();

    ReadContext &operator=(const ReadContext &) = delete;
    ReadContext &operator=(ReadContext &&) = delete;
};

struct WriteContext {
    ZSTD_CCtx *cctx;
    uint8_t *in_buffer;
    uint8_t *out_buffer;

    WriteContext();
    WriteContext(ZSTD_CCtx *ctx, uint8_t *in_buf, uint8_t *out_buf)
        : cctx(ctx), in_buffer(in_buf), out_buffer(out_buf) {}
    WriteContext(const WriteContext &) = delete;
    WriteContext(WriteContext &&other)
        : cctx(vull::exchange(other.cctx, nullptr)), in_buffer(vull::exchange(other.in_buffer, nullptr)),
          out_buffer(vull::exchange(other.out_buffer, nullptr)) {}
    ~WriteContext();

    WriteContext &operator=(const WriteContext &) = delete;
    WriteContext &operator=(WriteContext &&) = delete;
};

VULL_GLOBAL(thread_local Vector<ReadContext> s_read_contexts);
VULL_GLOBAL(thread_local Vector<WriteContext> s_write_contexts);

ReadContext::ReadContext() {
    dctx = ZSTD_createDCtx();
    in_buffer = new uint8_t[k_output_block_size];
    out_buffer = new uint8_t[k_input_block_size];
}

ReadContext::~ReadContext() {
    if (dctx != nullptr) {
        delete[] in_buffer;
        delete[] out_buffer;
        ZSTD_freeDCtx(dctx);
    }
}

WriteContext::WriteContext() {
    cctx = ZSTD_createCCtx();
    in_buffer = new uint8_t[k_input_block_size];
    out_buffer = new uint8_t[k_output_block_size];
    ZSTD_CCtx_setParameter(cctx, ZSTD_c_contentSizeFlag, 0);
    ZSTD_CCtx_setParameter(cctx, ZSTD_c_checksumFlag, 0);
    ZSTD_CCtx_setParameter(cctx, ZSTD_c_dictIDFlag, 0);
    // TODO: Use magicless format when no longer experimental/or block-level API if becomes undeprecated.
}

WriteContext::~WriteContext() {
    if (cctx != nullptr) {
        delete[] in_buffer;
        delete[] out_buffer;
        ZSTD_freeCCtx(cctx);
    }
}

} // namespace

ReadStream::ReadStream(UniquePtr<Stream> &&stream) : m_stream(vull::move(stream)) {
    if (s_read_contexts.empty()) {
        s_read_contexts.emplace();
    }

    auto context = s_read_contexts.take_last();
    m_dctx = vull::exchange(context.dctx, nullptr);
    m_in_buffer = vull::exchange(context.in_buffer, nullptr);
    m_out_buffer = vull::exchange(context.out_buffer, nullptr);
}

ReadStream::~ReadStream() {
    s_read_contexts.emplace(m_dctx, m_in_buffer, m_out_buffer);
}

Result<void, StreamError> ReadStream::read_next_block() {
    if (m_at_end) {
        return {};
    }

    // Read the worst-case block size.
    uint64_t chunk_size = VULL_TRY(m_stream->read({m_in_buffer, k_output_block_size}));
    if (chunk_size == 0) {
        // No data left.
        m_at_end = true;
        return {};
    }

    // Calculate the true compressed size.
    uint64_t compressed_size = ZSTD_findFrameCompressedSize(m_in_buffer, chunk_size);
    if (ZSTD_isError(compressed_size) != 0u) {
        return StreamError::Unknown;
    }

    // Decompress into another buffer.
    m_block_size = ZSTD_decompressDCtx(m_dctx, m_out_buffer, k_input_block_size, m_in_buffer, compressed_size);
    m_block_head = 0;

    VULL_ASSERT(m_block_size <= k_input_block_size);
    if (m_block_size != k_input_block_size) {
        // Block wasn't a full block, so this must be the last block.
        m_at_end = true;
        return {};
    }

    // Otherwise we read a full block, in which case there is potentially another block of data.
    VULL_TRY(m_stream->seek(-chunk_size + compressed_size, SeekMode::Add));
    auto next_block_offset = VULL_TRY(m_stream->read_be<uint64_t>());
    if (next_block_offset == UINT64_MAX) {
        // Edge case of a full block size multiple entry size, this is actually the last block.
        m_at_end = true;
        return {};
    }

    // There is another block, seek to its offset.
    VULL_TRY(m_stream->seek(next_block_offset, SeekMode::Set));
    return {};
}

Result<size_t, StreamError> ReadStream::read(Span<void> data) {
    size_t to_read = data.size();
    while (to_read > 0) {
        if (m_block_head == m_block_size) {
            VULL_TRY(read_next_block());
        }
        const auto to_copy = vull::min(to_read, m_block_size - m_block_head);
        if (to_copy == 0) {
            break;
        }
        memcpy(data.byte_offset(data.size() - to_read), m_out_buffer + m_block_head, to_copy);
        m_block_head += to_copy;
        to_read -= to_copy;
    }
    return data.size() - to_read;
}

Result<uint8_t, StreamError> ReadStream::read_byte() {
    if (m_block_head == m_block_size) {
        VULL_TRY(read_next_block());
        if (m_block_head == m_block_size) {
            return StreamError::Truncated;
        }
    }
    return m_out_buffer[m_block_head++];
}

WriteStream::WriteStream(Writer &writer, UniquePtr<Stream> &&stream, String &&name, EntryType type)
    : m_writer(writer), m_stream(vull::move(stream)) {
    m_entry.name = vull::move(name);
    m_entry.type = type;

    if (s_write_contexts.empty()) {
        s_write_contexts.emplace();
    }

    auto context = s_write_contexts.take_last();
    m_cctx = vull::exchange(context.cctx, nullptr);
    m_in_buffer = vull::exchange(context.in_buffer, nullptr);
    m_out_buffer = vull::exchange(context.out_buffer, nullptr);

    // Set ZSTD compression level.
    switch (writer.m_compression_level) {
    case CompressionLevel::Fast:
        ZSTD_CCtx_setParameter(m_cctx, ZSTD_c_compressionLevel, 12);
        break;
    case CompressionLevel::Normal:
        ZSTD_CCtx_setParameter(m_cctx, ZSTD_c_compressionLevel, 18);
        break;
    case CompressionLevel::Ultra:
        ZSTD_CCtx_setParameter(m_cctx, ZSTD_c_compressionLevel, 19);
        break;
    }
}

WriteStream::~WriteStream() {
    // Ensure that all data has been flushed.
    VULL_ASSERT(m_compress_head == 0);
    s_write_contexts.emplace(m_cctx, m_in_buffer, m_out_buffer);
}

Result<void, StreamError> WriteStream::flush_block() {
    VULL_ASSERT(m_compress_head != 0);
    m_entry.size += m_compress_head;

    // Compress accumulated data into output buffer.
    uint64_t compressed_size = ZSTD_compress2(m_cctx, m_out_buffer, k_output_block_size, m_in_buffer, m_compress_head);
    if (ZSTD_isError(compressed_size) != 0u) {
        return StreamError::Unknown;
    }
    m_compress_head = 0;

    // Allocate space for compressed data + block link offset.
    const uint64_t block_offset = m_writer.allocate_space(compressed_size + sizeof(uint64_t));
    if (m_entry.first_block == 0) {
        m_entry.first_block = block_offset;
    }

    // Write new block's offset to previous block's block link offset.
    if (m_block_link_offset != 0) {
        VULL_TRY(m_stream->seek(m_block_link_offset, SeekMode::Set));
        VULL_TRY(m_stream->write_be(block_offset));
    }

    // Write compressed data, followed by the sentinel last block value. Constexpr indirection is there to avoid an
    // erroneous clang-tidy warning.
    constexpr auto block_link_sentinel = UINT64_MAX;
    VULL_TRY(m_stream->seek(block_offset, SeekMode::Set));
    VULL_TRY(m_stream->write({m_out_buffer, compressed_size}));
    VULL_TRY(m_stream->write_be(block_link_sentinel));

    // Update block link offset.
    m_block_link_offset = block_offset + compressed_size;
    return {};
}

Result<void, StreamError> WriteStream::finish() {
    if (m_compress_head > 0) {
        VULL_TRY(flush_block());
    }
    m_writer.add_finished_entry(vull::move(m_entry));
    return {};
}

Result<void, StreamError> WriteStream::write(Span<const void> data) {
    for (size_t bytes_written = 0; bytes_written < data.size();) {
        const auto to_copy = vull::min(data.size() - bytes_written, k_input_block_size - m_compress_head);
        if (to_copy == 0) {
            VULL_TRY(flush_block());
            continue;
        }
        memcpy(m_in_buffer + m_compress_head, data.byte_offset(bytes_written), to_copy);
        m_compress_head += to_copy;
        bytes_written += to_copy;
    }
    return {};
}

Result<void, StreamError> WriteStream::write_byte(uint8_t byte) {
    if (m_compress_head == k_input_block_size) {
        VULL_TRY(flush_block());
    }
    m_in_buffer[m_compress_head++] = byte;
    return {};
}

} // namespace vull::vpak
