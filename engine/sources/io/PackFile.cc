#include <vull/io/PackFile.hh>

#include <vull/support/Array.hh>
#include <vull/support/Span.hh>
#include <vull/support/Vector.hh>

#include <iosfwd>
#include <stdexcept>

namespace {

constexpr std::size_t k_mesh_size = sizeof(std::uint32_t) + sizeof(std::uint64_t);

} // namespace

PackMesh::PackMesh(const Vector<std::uint8_t> &data) {
    m_index_count = (static_cast<std::uint32_t>(data[0]) << 0u) | (static_cast<std::uint32_t>(data[1]) << 8u) |
                    (static_cast<std::uint32_t>(data[2]) << 16u) | (static_cast<std::uint32_t>(data[3]) << 24u);
    m_index_offset = (static_cast<std::uint64_t>(data[4]) << 0u) | (static_cast<std::uint64_t>(data[5]) << 8u) |
                     (static_cast<std::uint64_t>(data[6]) << 16u) | (static_cast<std::uint64_t>(data[7]) << 24u) |
                     (static_cast<std::uint64_t>(data[8]) << 32u) | (static_cast<std::uint64_t>(data[9]) << 40u) |
                     (static_cast<std::uint64_t>(data[10]) << 48u) | (static_cast<std::uint64_t>(data[11]) << 56u);
}

const char *PackFile::entry_type_str(PackEntryType type) {
    switch (type) {
    case PackEntryType::VertexBuffer:
        return "vertex buffer";
    case PackEntryType::IndexBuffer:
        return "index buffer";
    case PackEntryType::Mesh:
        return "mesh";
    case PackEntryType::Shader:
        return "shader";
    default:
        return "unknown";
    }
}

PackFile::PackFile(std::FILE *file) : m_file(file) {
    std::fseek(file, 0, SEEK_SET);
}

std::size_t PackFile::read(Span<std::uint8_t> data) {
    return std::fread(data.data(), 1, data.size(), m_file);
}

bool PackFile::read_byte(std::uint8_t *byte) {
    return std::fread(byte, 1, 1, m_file) == 1;
}

std::uint16_t PackFile::read_header() {
    Array<std::uint8_t, 4> magic_bytes{};
    read(magic_bytes);
    if (magic_bytes[0] != 'V' || magic_bytes[1] != 'U' || magic_bytes[2] != 'L' || magic_bytes[3] != 'L') {
        throw std::runtime_error("Invalid magic number");
    }

    Array<std::uint8_t, 2> entry_count_bytes{};
    if (read(entry_count_bytes) != entry_count_bytes.size()) {
        throw std::runtime_error("Stream ended unexpectedly");
    }
    return (static_cast<std::uint16_t>(entry_count_bytes[0]) << 0u) |
           (static_cast<std::uint16_t>(entry_count_bytes[1]) << 8u);
}

PackEntry PackFile::read_entry() {
    PackEntryType type;
    if (!read_byte(reinterpret_cast<std::uint8_t *>(&type))) {
        throw std::runtime_error("Stream ended unexpectedly");
    }

    // Read total size of the entry.
    std::size_t size = 0;
    for (std::size_t size_byte_count = 0; size_byte_count < sizeof(std::size_t); size_byte_count++) {
        std::uint8_t byte;
        if (!read_byte(&byte)) {
            throw std::runtime_error("Stream ended unexpectedly");
        }
        size |= static_cast<std::size_t>(byte & 0x7fu) << (size_byte_count * 7u);
        if ((byte & 0x80u) == 0) {
            break;
        }
    }

    switch (type) {
    case PackEntryType::Mesh: {
        std::size_t name_length = size - k_mesh_size;
        std::string name(name_length, '\0');
        read({reinterpret_cast<std::uint8_t *>(name.data()), name_length});
        return {type, std::move(name), k_mesh_size};
    }
    case PackEntryType::Shader: {
        std::string name;
        while (true) {
            char ch;
            read_byte(reinterpret_cast<std::uint8_t *>(&ch));
            if (ch == '\0') {
                break;
            }
            name += ch;
        }
        return {type, std::move(name), size - name.length() - 1};
    }
    default:
        return {type, {}, size};
    }
}

Vector<std::uint8_t> PackFile::read_data(const PackEntry &entry) {
    Vector<std::uint8_t> data(entry.payload_size());
    read(data);
    return data;
}

void PackFile::skip_data(const PackEntry &entry) {
    std::fseek(m_file, static_cast<std::streamsize>(entry.payload_size()), SEEK_CUR);
}

void PackFile::write(Span<const std::uint8_t> data) {
    std::fwrite(data.data(), 1, data.size(), m_file);
}

void PackFile::write_byte(std::uint8_t byte) {
    std::fwrite(&byte, 1, 1, m_file);
}

void PackFile::write_header(std::uint16_t entry_count) {
    Array<std::uint8_t, 4> magic_bytes{'V', 'U', 'L', 'L'};
    write(magic_bytes);

    Array<std::uint8_t, 2> entry_count_bytes{static_cast<std::uint8_t>((entry_count >> 0u) & 0xffu),
                                             static_cast<std::uint8_t>((entry_count >> 8u) & 0xffu)};
    write(entry_count_bytes);
}

void PackFile::write_entry_header(PackEntryType type, std::uint64_t size) {
    write_byte(static_cast<std::uint8_t>(type));
    while (true) {
        std::uint8_t byte = (size & 0xffu) | 0x80u;
        size >>= 7u;
        if (size == 0) {
            write_byte(byte & 0x7fu);
            break;
        }
        write_byte(byte);
    }
}
