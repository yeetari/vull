#pragma once

#include <vull/core/builtin_components.hh>
#include <vull/ecs/component.hh>
#include <vull/support/string.hh>
#include <vull/support/utility.hh>

namespace vull {

struct Stream;

class Mesh {
    VULL_DECLARE_COMPONENT(BuiltinComponents::Mesh);

private:
    String m_vertex_data_name;
    String m_index_data_name;

public:
    static Mesh deserialise(Stream &stream);
    static void serialise(Mesh &mesh, Stream &stream);

    Mesh(String &&vertex_data_name, String &&index_data_name)
        : m_vertex_data_name(vull::move(vertex_data_name)), m_index_data_name(vull::move(index_data_name)) {}

    const String &vertex_data_name() const { return m_vertex_data_name; }
    const String &index_data_name() const { return m_index_data_name; }
};

} // namespace vull
