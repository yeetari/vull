#pragma once

#include <vull/core/PackFile.hh>
#include <vull/support/Optional.hh>
#include <vull/support/Span.hh>

#include <stdint.h>
#include <stdio.h>

using ZSTD_CCtx = struct ZSTD_CCtx_s;

namespace vull {

class PackWriter {
    FILE *const m_file;
    ZSTD_CCtx *const m_cctx;
    uint8_t *const m_buffer;

    off_t m_size_seek_back{0};
    size_t m_compressed_size{0};
    uint32_t m_entry_size{0};
    Optional<size_t> m_compress_head;

public:
    explicit PackWriter(FILE *file);
    PackWriter(const PackWriter &) = delete;
    PackWriter(PackWriter &&) = delete;
    ~PackWriter();

    PackWriter &operator=(const PackWriter &) = delete;
    PackWriter &operator=(PackWriter &&) = delete;

    void write_header();
    void start_entry(PackEntryType type, bool compressed);
    float end_entry();

    void write(Span<const void> data);
    void write_byte(uint8_t byte);
    void write_varint(uint32_t value);
};

} // namespace vull