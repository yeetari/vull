#pragma once

#include <vull/container/vector.hh>
#include <vull/support/result.hh>
#include <vull/support/span.hh>
#include <vull/support/stream.hh>
#include <vull/support/string.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/tasklet/mutex.hh>
#include <vull/vpak/pack_file.hh>

#include <stddef.h>
#include <stdint.h>

using ZSTD_CCtx = struct ZSTD_CCtx_s;

namespace vull::vpak {

class Writer;

enum class CompressionLevel {
    Fast,
    Normal,
    Ultra,
};

class WriteStream final : public Stream {
    Writer &m_writer;
    UniquePtr<Stream> m_stream;
    Entry &m_entry;
    ZSTD_CCtx *m_cctx{nullptr};
    uint8_t *m_buffer{nullptr};
    size_t m_block_link_offset{0};
    uint32_t m_compress_head{0};
    uint32_t m_compressed_size{0};

    Result<void, StreamError> flush_block();

public:
    WriteStream(Writer &writer, UniquePtr<Stream> &&stream, Entry &entry);
    WriteStream(const WriteStream &) = delete;
    WriteStream(WriteStream &&) = delete;
    ~WriteStream() override;

    WriteStream &operator=(const WriteStream &) = delete;
    WriteStream &operator=(WriteStream &&) = delete;

    float finish();
    Result<void, StreamError> write(Span<const void> data) override;
    Result<void, StreamError> write_byte(uint8_t byte) override;
};

class Writer {
    friend WriteStream;

private:
    UniquePtr<Stream> m_stream;
    const CompressionLevel m_clevel;
    Vector<UniquePtr<Entry>> m_entries;
    Mutex m_mutex;

    Result<uint64_t, StreamError> allocate(size_t size);
    Result<void, StreamError> read_existing();
    void write_entry_table();

public:
    Writer(UniquePtr<Stream> &&stream, CompressionLevel clevel);
    Writer(const Writer &) = delete;
    Writer(Writer &&) = delete;
    ~Writer() = default;

    Writer &operator=(const Writer &) = delete;
    Writer &operator=(Writer &&) = delete;

    uint64_t finish();
    WriteStream start_entry(String name, EntryType type);
};

} // namespace vull::vpak
