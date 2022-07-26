#pragma once

#include <vull/support/StringView.hh>
#include <vull/vpak/Writer.hh>

#include <stdint.h>

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
    vull::StringView m_json;
    const uint8_t *m_binary_blob;

public:
    bool parse_glb(vull::StringView input_path);
    bool convert(vull::vpak::Writer &pack_writer, bool reproducible);

    vull::StringView json() const { return m_json; }
};
