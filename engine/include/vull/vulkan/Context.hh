#pragma once

#include <vull/support/Vector.hh>
#include <vull/vulkan/ContextTable.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stdint.h>

namespace vull {

enum class MemoryType {
    DeviceLocal,
    HostVisible,
    Staging,
};

class VkContext : public vkb::ContextTable {
    Vector<vkb::MemoryType> m_memory_types;
    Vector<vkb::QueueFamilyProperties> m_queue_families;

    uint32_t find_memory_type_index(const vkb::MemoryRequirements &requirements, MemoryType type) const;

public:
    VkContext();
    VkContext(const VkContext &) = delete;
    VkContext(VkContext &&) = delete;
    ~VkContext();

    VkContext &operator=(const VkContext &) = delete;
    VkContext &operator=(VkContext &&) = delete;

    vkb::DeviceMemory allocate_memory(const vkb::MemoryRequirements &requirements, MemoryType type) const;
    const Vector<vkb::QueueFamilyProperties> &queue_families() const { return m_queue_families; }
};

} // namespace vull
