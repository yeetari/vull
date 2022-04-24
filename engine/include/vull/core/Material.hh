#pragma once

#include <vull/core/BuiltinComponents.hh>
#include <vull/ecs/Component.hh>

#include <stdint.h>

namespace vull {

class Material {
    VULL_DECLARE_COMPONENT(BuiltinComponents::Material);

private:
    uint32_t m_albedo_index;
    uint32_t m_normal_index;

public:
    Material(uint32_t albedo_index, uint32_t normal_index)
        : m_albedo_index(albedo_index), m_normal_index(normal_index) {}

    uint32_t albedo_index() const { return m_albedo_index; }
    uint32_t normal_index() const { return m_normal_index; }
};

} // namespace vull
