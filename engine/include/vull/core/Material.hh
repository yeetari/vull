#pragma once

#include <vull/core/BuiltinComponents.hh>
#include <vull/ecs/Component.hh>
#include <vull/support/Function.hh>
#include <vull/support/String.hh>

#include <stdint.h>

namespace vull {

class Material {
    VULL_DECLARE_COMPONENT(BuiltinComponents::Material);

private:
    String m_albedo_name;
    String m_normal_name;

public:
    static Material deserialise(const Function<uint8_t()> &read_byte);
    static void serialise(Material &material, const Function<void(uint8_t)> &write_byte);

    Material(String &&albedo_name, String &&normal_name)
        : m_albedo_name(vull::move(albedo_name)), m_normal_name(vull::move(normal_name)) {}

    const String &albedo_name() const { return m_albedo_name; }
    const String &normal_name() const { return m_normal_name; }
};

inline Material Material::deserialise(const Function<uint8_t()> &read_byte) {
    String albedo_name(read_byte());
    for (char &ch : albedo_name) {
        ch = static_cast<char>(read_byte());
    }
    String normal_name(read_byte());
    for (char &ch : normal_name) {
        ch = static_cast<char>(read_byte());
    }
    return {vull::move(albedo_name), vull::move(normal_name)};
}

inline void Material::serialise(Material &material, const Function<void(uint8_t)> &write_byte) {
    const auto albedo_length = static_cast<uint8_t>(material.m_albedo_name.length());
    write_byte(albedo_length);
    for (uint8_t i = 0; i < albedo_length; i++) {
        write_byte(material.m_albedo_name.view().as<const uint8_t>()[i]);
    }

    const auto normal_length = static_cast<uint8_t>(material.m_normal_name.length());
    write_byte(normal_length);
    for (uint8_t i = 0; i < normal_length; i++) {
        write_byte(material.m_normal_name.view().as<const uint8_t>()[i]);
    }
}

} // namespace vull
