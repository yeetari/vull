#pragma once

#include <vull/core/BuiltinComponents.hh>
#include <vull/ecs/Component.hh>
#include <vull/support/Function.hh>
#include <vull/support/String.hh>

#include <stdint.h>

namespace vull {

class Mesh {
    VULL_DECLARE_COMPONENT(BuiltinComponents::Mesh);

private:
    String m_vertex_data_name;
    String m_index_data_name;

public:
    static Mesh deserialise(const Function<uint8_t()> &read_byte);

    // TODO(stream-api): Take in a Stream.
    static void serialise(Mesh &mesh, const Function<void(uint8_t)> &write_byte);

    Mesh(String &&vertex_data_name, String &&index_data_name)
        : m_vertex_data_name(vull::move(vertex_data_name)), m_index_data_name(vull::move(index_data_name)) {}

    const String &vertex_data_name() const { return m_vertex_data_name; }
    const String &index_data_name() const { return m_index_data_name; }
};

inline Mesh Mesh::deserialise(const Function<uint8_t()> &read_byte) {
    String vertex_data_name(read_byte());
    for (char &ch : vertex_data_name) {
        ch = static_cast<char>(read_byte());
    }
    String index_data_name(read_byte());
    for (char &ch : index_data_name) {
        ch = static_cast<char>(read_byte());
    }
    return {vull::move(vertex_data_name), vull::move(index_data_name)};
}

inline void Mesh::serialise(Mesh &mesh, const Function<void(uint8_t)> &write_byte) {
    const auto vertex_length = static_cast<uint8_t>(mesh.m_vertex_data_name.length());
    write_byte(vertex_length);
    for (uint8_t i = 0; i < vertex_length; i++) {
        write_byte(mesh.m_vertex_data_name.view().as<const uint8_t>()[i]);
    }

    const auto index_length = static_cast<uint8_t>(mesh.m_index_data_name.length());
    write_byte(index_length);
    for (uint8_t i = 0; i < index_length; i++) {
        write_byte(mesh.m_index_data_name.view().as<const uint8_t>()[i]);
    }
}

} // namespace vull
