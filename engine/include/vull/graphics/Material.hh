#pragma once

#include <vull/core/BuiltinComponents.hh>
#include <vull/ecs/Component.hh>
#include <vull/support/String.hh>
#include <vull/support/Utility.hh>

namespace vull {

struct Stream;

class Material {
    VULL_DECLARE_COMPONENT(BuiltinComponents::Material);

private:
    String m_albedo_name;
    String m_normal_name;

public:
    static Material deserialise(Stream &stream);
    static void serialise(Material &material, Stream &stream);

    Material() = default;
    Material(const String &albedo_name, const String &normal_name)
        : m_albedo_name(albedo_name), m_normal_name(normal_name) {}
    Material(String &&albedo_name, String &&normal_name)
        : m_albedo_name(vull::move(albedo_name)), m_normal_name(vull::move(normal_name)) {}

    const String &albedo_name() const { return m_albedo_name; }
    const String &normal_name() const { return m_normal_name; }
};

} // namespace vull
