#pragma once

#include <vull/core/BuiltinComponents.hh>
#include <vull/ecs/Component.hh>

#include <stdint.h>

namespace vull {

class Mesh {
    VULL_DECLARE_COMPONENT(BuiltinComponents::Mesh);

private:
    uint32_t m_index;
    uint32_t m_index_count;

public:
    Mesh(uint32_t index, uint32_t index_count) : m_index(index), m_index_count(index_count) {}

    uint32_t index() const { return m_index; }
    uint32_t index_count() const { return m_index_count; }
};

} // namespace vull
