#pragma once

#include <vulkan/vulkan_core.h>

#include <cstdint>

class Mesh {
    const std::uint32_t m_index_count;
    const VkDeviceSize m_index_offset;

public:
    Mesh(std::uint32_t index_count, VkDeviceSize index_offset)
        : m_index_count(index_count), m_index_offset(index_offset) {}

    std::uint32_t index_count() const { return m_index_count; }
    VkDeviceSize index_offset() const { return m_index_offset; }
};
