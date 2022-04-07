#pragma once

#include <vull/core/BuiltinComponents.hh>
#include <vull/ecs/Component.hh>

#include <stdint.h>

namespace vull {

class Mesh {
    VULL_DECLARE_COMPONENT(BuiltinComponents::Mesh);

private:
    uint32_t m_index;

public:
    explicit Mesh(uint32_t index) : m_index(index) {}

    uint32_t index() const { return m_index; }
};

} // namespace vull
