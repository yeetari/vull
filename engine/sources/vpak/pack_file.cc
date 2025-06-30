#include <vull/vpak/pack_file.hh>

#include <vull/container/vector.hh>
#include <vull/platform/file.hh>
#include <vull/platform/file_stream.hh>
#include <vull/support/optional.hh>
#include <vull/support/perfect_hasher.hh>
#include <vull/support/result.hh>
#include <vull/support/stream.hh>
#include <vull/support/string.hh>
#include <vull/support/string_view.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>
#include <vull/vpak/defs.hh>
#include <vull/vpak/stream.hh>
#include <vull/vpak/writer.hh>

#include <stdint.h>

namespace vull::vpak {

Result<PackFile, platform::OpenError, StreamError, VpakError> PackFile::open(String path) {
    auto file_or_error = platform::open_file(path, platform::OpenMode::Read);
    auto file = file_or_error.to_optional().value_or(platform::File());
    if (!file && file_or_error.error() != platform::OpenError::NonExistent) {
        return file_or_error.error();
    }

    // TODO: Don't read if file empty (e.g. if created by touch).
    const bool should_read = !!file;
    PackFile pack_file(vull::move(path), vull::move(file));
    if (should_read) {
        VULL_TRY(pack_file.read_existing());
    }
    return pack_file;
}

Result<void, StreamError, VpakError> PackFile::read_existing() {
    auto stream = m_file.create_stream();
    if (VULL_TRY(stream.read_be<uint32_t>()) != k_magic_number) {
        return VpakError::BadMagic;
    }
    if (VULL_TRY(stream.read_be<uint32_t>()) != 1u) {
        return VpakError::BadVersion;
    }
    if (VULL_TRY(stream.read_be<uint32_t>()) != 0u) {
        return VpakError::BadFlags;
    }

    const auto entry_count = VULL_TRY(stream.read_be<uint32_t>());
    if (entry_count > k_entry_limit) {
        return VpakError::TooManyEntries;
    }

    const auto entry_table_offset = VULL_TRY(stream.read_be<uint64_t>());
    VULL_TRY(stream.seek(entry_table_offset, SeekMode::Set));

    Vector<int32_t> seeds;
    seeds.ensure_capacity(entry_count);
    for (uint32_t i = 0; i < entry_count; i++) {
        const auto seed = VULL_EXPECT(stream.read_be<uint32_t>());
        seeds.push(static_cast<int32_t>(seed));
    }
    m_phf = {vull::move(seeds)};

    m_entries.ensure_size(entry_count);
    for (auto &entry : m_entries) {
        entry.type = static_cast<EntryType>(VULL_TRY(stream.read_byte()));
        entry.name = VULL_TRY(stream.read_string());
        entry.size = VULL_TRY(stream.read_varint<uint32_t>());
        entry.first_block = VULL_TRY(stream.read_varint<uint64_t>());
    }
    return {};
}

bool PackFile::exists(StringView name) const {
    return !m_entries.empty() && m_entries[m_phf.hash(name)].name.view() == name;
}

UniquePtr<ReadStream> PackFile::open_entry(StringView name) const {
    if (m_entries.empty()) {
        return {};
    }
    const auto &entry = m_entries[m_phf.hash(name)];
    auto stream = m_file.create_stream();
    VULL_ASSUME(stream.seek(entry.first_block, SeekMode::Set));
    return entry.name.view() == name ? vull::make_unique<ReadStream>(vull::adopt_unique(vull::move(stream)))
                                     : UniquePtr<ReadStream>();
}

Optional<Entry> PackFile::stat(StringView name) const {
    if (m_entries.empty()) {
        return vull::nullopt;
    }
    const auto &entry = m_entries[m_phf.hash(name)];
    return entry.name.view() == name ? entry : Optional<Entry>();
}

Result<Writer, platform::FileError, platform::OpenError> PackFile::make_writer(CompressionLevel compression_level) {
    auto write_file = VULL_TRY(platform::open_file(
        platform::dir_path(m_path), platform::OpenModes(platform::OpenMode::TempFile, platform::OpenMode::Write)));
    int64_t dst_offset = k_header_size;
    if (m_file) {
        // Copy existing entry data to write file, skipping the old header.
        // TODO: Don't copy old entry table.
        // TODO: Investigate vacuuming, i.e. removing old unreferenced entry data.
        int64_t src_offset = k_header_size;
        VULL_TRY(m_file.copy_to(write_file, src_offset, dst_offset));
    }
    return Writer(vull::move(write_file), static_cast<uint64_t>(dst_offset), compression_level);
}

Result<uint64_t, platform::FileError, platform::OpenError, StreamError> PackFile::finish_writing(Writer &&writer) {
    // TODO: Re-assign to m_entries after write to disk is successful.
    const auto bytes_written = VULL_TRY(writer.finish(m_entries));
    m_file = vull::move(writer.m_write_file);

    // Sync temporary file data.
    VULL_TRY(m_file.sync());

    // Open parent directory now.
    auto parent_directory = VULL_TRY(platform::open_file(
        platform::dir_path(m_path), platform::OpenModes(platform::OpenMode::Read, platform::OpenMode::Directory)));

    // Can't use renameat with an O_TMPFILE, so this isn't truly atomic :(.
    if (auto result = platform::unlink_path(m_path);
        result.is_error() && result.error() != platform::FileError::NonExistent) {
        return result.error();
    }
    VULL_TRY(m_file.link_to(m_path));

    // Sync parent directory. Don't signal failure if this doesn't work, since we've already done the rename now.
    static_cast<void>(parent_directory.sync());
    return bytes_written;
}

} // namespace vull::vpak
