#pragma once

#include <vull/support/result.hh>
#include <vull/support/span.hh>
#include <vull/support/stream.hh>
#include <vull/support/string.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/vpak/defs.hh>

#include <stddef.h>
#include <stdint.h>

using ZSTD_CCtx = struct ZSTD_CCtx_s;
using ZSTD_DCtx = struct ZSTD_DCtx_s;

namespace vull::vpak {

class Writer;

class ReadStream final : public Stream {
    UniquePtr<Stream> m_stream;

    ZSTD_DCtx *m_dctx{nullptr};
    uint8_t *m_in_buffer{nullptr};
    uint8_t *m_out_buffer{nullptr};

    uint64_t m_block_size{0};
    uint64_t m_block_head{0};
    bool m_at_end{false};

    Result<void, StreamError> read_next_block();

public:
    ReadStream(UniquePtr<Stream> &&stream);
    ReadStream(const ReadStream &) = delete;
    ReadStream(ReadStream &&) = delete;
    ~ReadStream() override;

    ReadStream &operator=(const ReadStream &) = delete;
    ReadStream &operator=(ReadStream &&) = delete;

    Result<size_t, StreamError> read(Span<void> data) override;
    Result<uint8_t, StreamError> read_byte() override;
};

class WriteStream final : public Stream {
    Writer &m_writer;
    UniquePtr<Stream> m_stream;

    ZSTD_CCtx *m_cctx{nullptr};
    uint8_t *m_in_buffer{nullptr};
    uint8_t *m_out_buffer{nullptr};

    uint64_t m_block_link_offset{0};
    uint32_t m_compress_head{0};
    Entry m_entry{};

    Result<void, StreamError> flush_block();

public:
    WriteStream(Writer &writer, UniquePtr<Stream> &&stream, String &&name, EntryType type);
    WriteStream(const WriteStream &) = delete;
    WriteStream(WriteStream &&) = delete;
    ~WriteStream() override;

    WriteStream &operator=(const WriteStream &) = delete;
    WriteStream &operator=(WriteStream &&) = delete;

    Result<void, StreamError> finish();
    Result<void, StreamError> write(Span<const void> data) override;
    Result<void, StreamError> write_byte(uint8_t byte) override;
};

} // namespace vull::vpak
