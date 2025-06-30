#pragma once

#include <vull/container/vector.hh>
#include <vull/platform/file.hh>
#include <vull/support/optional.hh>
#include <vull/support/perfect_hasher.hh>
#include <vull/support/result.hh>
#include <vull/support/string.hh>
#include <vull/support/string_view.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>
#include <vull/vpak/defs.hh>

#include <stdint.h>

/*
 * struct {
 *     u32 magic = 0x8186564b;
 *     u32 version = 1;
 *     u32 flags = 0;
 *     u32 entry_count;
 *     u64 entry_table_offset;
 *     u8 block_data[];
 * };
 *
 * struct EntryHeader {
 *     EntryType(u8) type;
 *     v64 name_length;
 *     u8 name[name_length];
 *     v32 size; // uncompressed size in bytes
 *     v64 first_block;
 * };
 *
 * struct EntryTable {
 *     u32 hash_seeds[entry_count];
 *     EntryHeader entries[entry_count];
 * };
 *
 * struct Blob(type: 0) {
 *     u8 data[size];
 * };
 *
 * struct Image(type: 1) {
 *     ImageFormat(u8) format;
 *     ImageFilter(u8) mag_filter;
 *     ImageFilter(u8) min_filter;
 *     ImageWrapMode(u8) wrap_u;
 *     ImageWrapMode(u8) wrap_v;
 *     v32 width;
 *     v32 height;
 *     v32 mip_count;
 *     u8 mip_data[];
 * };
 *
 * // Handled in World::serialise and World::deserialise
 * struct World(type: 2) {
 *     struct ComponentSet {
 *         v32 entity_count;
 *         u8 serialised_data[];
 *         v32 entity_ids[entity_count];
 *     };
 *     v32 entity_count;
 *     v32 set_count;
 *     ComponentSet sets[set_count];
 * };
 */

namespace vull {

enum class StreamError;

} // namespace vull

namespace vull::vpak {

class ReadStream;
class Writer;

class PackFile {
    String m_path;
    platform::File m_file;
    Vector<Entry> m_entries;
    PerfectHasher m_phf;

    PackFile(String &&path, platform::File &&file) : m_path(vull::move(path)), m_file(vull::move(file)) {}

    Result<void, StreamError, VpakError> read_existing();

public:
    static Result<PackFile, platform::OpenError, StreamError, VpakError> open(String path);

    PackFile(const PackFile &) = delete;
    PackFile(PackFile &&) = default;
    ~PackFile() = default;

    PackFile &operator=(const PackFile &) = delete;
    PackFile &operator=(PackFile &&) = delete;

    bool exists(StringView name) const;
    UniquePtr<ReadStream> open_entry(StringView name) const;
    Optional<Entry> stat(StringView name) const;

    /**
     * Makes and returns a new Writer for this pack file with the given compression level.
     *
     * @param compression_level the compression level to use
     * @return Writer if successful
     * @return FileError if copying existing entry data to a new file failed
     * @return OpenError if creating a temporary file failed
     */
    Result<Writer, platform::FileError, platform::OpenError> make_writer(CompressionLevel compression_level);

    /**
     * Commits the changes made by the given writer to this PackFile object, and writes out a new vpak to disk\. In the
     * event of an error, the existing file on disk is not touched.
     *
     * @param writer a moved reference to a writer
     * @return uint64_t the number of bytes written to disk, if successful
     * @return FileError if syncing or renaming the file on disk failed
     * @return OpenError if opening the parent directory failed
     * @return StreamError if writing the header or entry table to disk failed
     */
    Result<uint64_t, platform::FileError, platform::OpenError, StreamError> finish_writing(Writer &&writer);

    const Vector<Entry> &entries() const { return m_entries; }
};

} // namespace vull::vpak
