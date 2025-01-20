#include <vull/vpak/writer.hh>

#include <vull/container/vector.hh>
#include <vull/core/log.hh>
#include <vull/platform/file.hh>
#include <vull/platform/file_stream.hh>
#include <vull/support/algorithm.hh>
#include <vull/support/atomic.hh>
#include <vull/support/perfect_hasher.hh>
#include <vull/support/result.hh>
#include <vull/support/scoped_lock.hh>
#include <vull/support/stream.hh>
#include <vull/support/string.hh>
#include <vull/support/string_view.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>
#include <vull/tasklet/mutex.hh>
#include <vull/vpak/defs.hh>
#include <vull/vpak/stream.hh>

#include <stdint.h>

namespace vull::vpak {

void Writer::add_finished_entry(Entry &&entry) {
    ScopedLock lock(m_mutex);
    m_new_entries.push(vull::move(entry));
}

uint64_t Writer::allocate_space(uint64_t size) {
    return m_head.fetch_add(size);
}

Result<uint64_t, StreamError> Writer::finish(Vector<Entry> &entries) {
    auto header_stream = m_write_file.create_stream();
    auto table_stream = m_write_file.create_stream();
    const auto entry_table_offset = VULL_TRY(table_stream.seek(0, SeekMode::End));

    // Append new entries.
    // TODO: This doesn't handle duplicate entries.
    entries.extend(m_new_entries);

    // Write header.
    VULL_TRY(header_stream.write_be<uint32_t>(k_magic_number));
    VULL_TRY(header_stream.write_be<uint32_t>(1u));
    VULL_TRY(header_stream.write_be<uint32_t>(0u));
    VULL_TRY(header_stream.write_be<uint32_t>(entries.size()));
    VULL_TRY(header_stream.write_be<uint64_t>(entry_table_offset));

    // Write entry table.
    vull::debug("[vpak] Writing entry table ({} new entries, {} total)", m_new_entries.size(), entries.size());

    Vector<StringView> keys;
    keys.ensure_capacity(entries.size());
    for (const auto &entry : entries) {
        keys.push(entry.name);
    }

    PerfectHasher phf;
    phf.build(keys);
    vull::sort(entries, [&](const auto &lhs, const auto &rhs) {
        return phf.hash(lhs.name) > phf.hash(rhs.name);
    });

    for (int32_t seed : phf.seeds()) {
        VULL_TRY(table_stream.write_be(static_cast<uint32_t>(seed)));
    }
    for (const auto &entry : entries) {
        VULL_TRY(table_stream.write_byte(static_cast<uint8_t>(entry.type)));
        VULL_TRY(table_stream.write_string(entry.name));
        VULL_TRY(table_stream.write_varint(entry.size));
        VULL_TRY(table_stream.write_varint(entry.first_block));
    }
    return VULL_TRY(table_stream.seek(0, SeekMode::Add));
}

WriteStream Writer::add_entry(String name, EntryType type) {
    auto stream = vull::adopt_unique(m_write_file.create_stream());
    return {*this, vull::move(stream), vull::move(name), type};
}

} // namespace vull::vpak
