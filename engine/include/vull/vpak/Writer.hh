#pragma once

#include <vull/support/Optional.hh>
#include <vull/support/Span.hh>
#include <vull/support/String.hh>
#include <vull/support/UniquePtr.hh> // IWYU pragma: keep
#include <vull/support/Vector.hh>
#include <vull/vpak/PackFile.hh>

#include <pthread.h>
#include <stdint.h>
#include <sys/types.h>

namespace vull::vpak {

class Writer;

enum class CompressionLevel {
    Fast,
    Normal,
    Ultra,
};

class WriteStream {
    Writer &m_writer;
    Entry &m_entry;
    off64_t m_block_link_offset{0};
    off64_t m_compress_head{0};
    uint32_t m_compressed_size{0};

    void flush_block();

public:
    WriteStream(Writer &writer, Entry &entry);
    WriteStream(const WriteStream &) = delete;
    WriteStream(WriteStream &&) = delete;
    ~WriteStream();

    WriteStream &operator=(const WriteStream &) = delete;
    WriteStream &operator=(WriteStream &&) = delete;

    float finish();
    void write(Span<const void> data);
    void write_byte(uint8_t byte);
    void write_varint(uint64_t value);
};

class Writer {
    friend WriteStream;

private:
    const int m_fd;
    const CompressionLevel m_clevel;
    Vector<UniquePtr<Entry>> m_entries;
    pthread_mutex_t m_mutex;

    off64_t allocate(off64_t size);
    void write_entry_table();
    void write_raw(Span<const void>, Optional<off64_t> offset = {}) const;
    void write_raw(uint8_t, Optional<off64_t> offset = {}) const;
    void write_raw(uint32_t, Optional<off64_t> offset = {}) const;
    void write_raw(uint64_t, Optional<off64_t> offset = {}) const;

public:
    Writer(const String &path, CompressionLevel clevel);
    Writer(const Writer &) = delete;
    Writer(Writer &&) = delete;
    ~Writer();

    Writer &operator=(const Writer &) = delete;
    Writer &operator=(Writer &&) = delete;

    uint64_t finish();
    WriteStream start_entry(String name, EntryType type);
};

} // namespace vull::vpak
