#pragma once

#include <vull/container/vector.hh>
#include <vull/support/optional.hh>
#include <vull/support/perfect_hasher.hh>
#include <vull/support/result.hh>
#include <vull/support/span.hh>
#include <vull/support/stream.hh>
#include <vull/support/string_view.hh>
#include <vull/support/unique_ptr.hh>

#include <stddef.h>
#include <stdint.h>

using ZSTD_DCtx = struct ZSTD_DCtx_s;

namespace vull {

class File;

} // namespace vull

namespace vull::vpak {

struct Entry;

class ReadStream final : public Stream {
    const Span<uint8_t> m_data;
    size_t m_block_start;
    size_t m_compressed_size{0};
    size_t m_offset{0};
    ZSTD_DCtx *m_dctx{nullptr};
    uint8_t *m_buffer{nullptr};

public:
    ReadStream(Span<uint8_t> data, size_t first_block);
    ReadStream(const ReadStream &) = delete;
    ReadStream(ReadStream &&) = delete;
    ~ReadStream() override;

    ReadStream &operator=(const ReadStream &) = delete;
    ReadStream &operator=(ReadStream &&) = delete;

    // TODO: override read_byte for a faster path.
    Result<size_t, StreamError> read(Span<void> data) override;
};

class Reader {
    Span<uint8_t> m_data;
    Vector<Entry> m_entries;
    PerfectHasher m_phf;

public:
    explicit Reader(File &&file);
    Reader(const Reader &) = delete;
    Reader(Reader &&) = delete;
    ~Reader();

    Reader &operator=(const Reader &) = delete;
    Reader &operator=(Reader &&) = delete;

    bool exists(StringView name) const;
    UniquePtr<ReadStream> open(StringView name) const;
    Optional<Entry> stat(StringView name) const;
    const Vector<Entry> &entries() const { return m_entries; }
};

} // namespace vull::vpak
