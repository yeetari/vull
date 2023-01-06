#pragma once

#include <vull/support/StringView.hh>
#include <vull/vpak/Writer.hh>

#include <stdint.h>

namespace vull {

class FileMmap {
    uint8_t *m_data{nullptr};
    size_t m_size{0};

public:
    FileMmap() = default;
    FileMmap(int fd, size_t size);
    FileMmap(const FileMmap &) = delete;
    FileMmap(FileMmap &&) = delete;
    ~FileMmap();

    FileMmap &operator=(const FileMmap &) = delete;
    FileMmap &operator=(FileMmap &&);

    explicit operator bool() const { return m_data != nullptr; }
    const uint8_t &operator[](size_t offset) const { return m_data[offset]; }
};

class GltfParser {
    FileMmap m_data;
    StringView m_json;
    const uint8_t *m_binary_blob;

public:
    bool parse_glb(StringView input_path);
    bool convert(vpak::Writer &pack_writer, bool max_resolution, bool reproducible);

    StringView json() const { return m_json; }
};

} // namespace vull
