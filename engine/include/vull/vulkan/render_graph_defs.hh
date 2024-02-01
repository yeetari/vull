#pragma once

#include <stdint.h>

namespace vull::vk {

class ResourceId {
    uint16_t m_physical_index;
    uint16_t m_virtual_index;

public:
    ResourceId(uint16_t physical_index, uint16_t virtual_index)
        : m_physical_index(physical_index), m_virtual_index(virtual_index) {}

    uint16_t physical_index() const { return m_physical_index; }
    uint16_t virtual_index() const { return m_virtual_index; }
};

// TODO: Wrangle IWYU to have a forward declaration of RenderGraph here.

} // namespace vull::vk
