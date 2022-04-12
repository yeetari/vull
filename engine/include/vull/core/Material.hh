#pragma once

#include <vull/core/BuiltinComponents.hh>
#include <vull/ecs/Component.hh>

#include <stdint.h>

namespace vull {

class Material {
    VULL_DECLARE_COMPONENT(BuiltinComponents::Material);

private:
    uint32_t m_albedo_index;

public:
    explicit Material(uint32_t albedo_index) : m_albedo_index(albedo_index) {}

    uint32_t albedo_index() const { return m_albedo_index; }
};

} // namespace vull
