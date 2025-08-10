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
    String m_data_path;

public:
    static Mesh deserialise(Stream &stream);
    static void serialise(Mesh &mesh, Stream &stream);

    explicit Mesh(String &&data_path) : m_data_path(vull::move(data_path)) {}

    const String &data_path() const { return m_data_path; }
};

} // namespace vull
