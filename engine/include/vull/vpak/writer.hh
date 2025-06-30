#pragma once

#include <vull/container/vector.hh>
#include <vull/platform/file.hh>
#include <vull/support/atomic.hh>
#include <vull/support/result.hh>
#include <vull/support/stream.hh>
#include <vull/support/string.hh>
#include <vull/support/utility.hh>
#include <vull/tasklet/mutex.hh>
#include <vull/vpak/defs.hh>

#include <stdint.h>

namespace vull::vpak {

class PackFile;
class WriteStream;

class Writer {
    friend PackFile;
    friend WriteStream;

private:
    platform::File m_write_file;
    Atomic<uint64_t> m_head;
    Vector<Entry> m_new_entries;
    tasklet::Mutex m_mutex;
    const CompressionLevel m_compression_level;

    Writer(platform::File &&write_file, uint64_t head, CompressionLevel compression_level)
        : m_write_file(vull::move(write_file)), m_head(head), m_compression_level(compression_level) {}

    void add_finished_entry(Entry &&entry);
    uint64_t allocate_space(uint64_t size);
    Result<uint64_t, StreamError> finish(Vector<Entry> &entries);

public:
    Writer(const Writer &) = delete;
    Writer(Writer &&other)
        : m_write_file(vull::move(other.m_write_file)), m_head(other.m_head.exchange(0)),
          m_compression_level(other.m_compression_level) {}
    ~Writer() = default;

    Writer &operator=(const Writer &) = delete;
    Writer &operator=(Writer &&) = delete;

    WriteStream add_entry(String name, EntryType type);
};

} // namespace vull::vpak
