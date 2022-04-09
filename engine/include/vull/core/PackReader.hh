#pragma once

#include <vull/core/PackFile.hh>
#include <vull/support/Optional.hh>
#include <vull/support/Span.hh>

#include <stdint.h>
#include <stdio.h>

using ZSTD_DCtx = struct ZSTD_DCtx_s;

namespace vull {

struct PackEntry {
    uint32_t size;
    PackEntryType type;
};

class PackReader {
    FILE *const m_file;
    ZSTD_DCtx *const m_dctx;
    uint8_t *const m_buffer;
    size_t m_size{0};
    uint8_t *m_data{nullptr};

    size_t m_head{0};
    size_t m_compressed_size{0};
    bool m_compressed{false};

public:
    explicit PackReader(FILE *file);
    PackReader(const PackReader &) = delete;
    PackReader(PackReader &&) = delete;
    ~PackReader();

    PackReader &operator=(const PackReader &) = delete;
    PackReader &operator=(PackReader &&) = delete;

    void read_header();
    Optional<PackEntry> read_entry();

    void read(Span<void> data);
    uint8_t read_byte();
    uint32_t read_varint();
};

} // namespace vull
