#pragma once

#include <vull/support/Span.hh> // IWYU pragma: keep
#include <vull/support/Vector.hh>

#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>

enum class PackEntryType : std::uint8_t {
    VertexBuffer = 0,
    IndexBuffer = 1,
    Mesh = 2,
    Shader = 3,
};

class PackEntry {
    const PackEntryType m_type;
    const std::string m_name;
    const std::size_t m_payload_size;

public:
    PackEntry(PackEntryType type, std::string &&name, std::size_t payload_size)
        : m_type(type), m_name(std::move(name)), m_payload_size(payload_size) {}

    PackEntryType type() const { return m_type; }
    const std::string &name() const { return m_name; }
    std::size_t payload_size() const { return m_payload_size; }
};

class PackMesh {
    std::uint32_t m_index_count;
    std::uint64_t m_index_offset;

public:
    explicit PackMesh(const Vector<std::uint8_t> &data);

    std::uint32_t index_count() const { return m_index_count; }
    std::uint64_t index_offset() const { return m_index_offset; }
};

class PackFile {
    std::FILE *const m_file;

public:
    static const char *entry_type_str(PackEntryType type);

    explicit PackFile(std::FILE *file);

    std::size_t read(Span<std::uint8_t> data);
    bool read_byte(std::uint8_t *byte);
    std::uint16_t read_header();
    PackEntry read_entry();
    Vector<std::uint8_t> read_data(const PackEntry &entry);
    void skip_data(const PackEntry &entry);

    void write(Span<const std::uint8_t> data);
    void write_byte(std::uint8_t byte);
    void write_header(std::uint16_t entry_count);
    void write_entry_header(PackEntryType type, std::uint64_t size);
};
