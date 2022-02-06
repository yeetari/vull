#pragma once

#include <vull/support/Vector.hh>
#include <vull/vulkan/ContextTable.hh>

#include <stdint.h>
#include <vulkan/vulkan_core.h>

namespace vull {

enum class MemoryType {
    DeviceLocal,
    HostVisible,
};

class Context : public ContextTable {
    Vector<VkMemoryType> m_memory_types;
    Vector<VkQueueFamilyProperties> m_queue_families;

    uint32_t find_memory_type_index(const VkMemoryRequirements &requirements, MemoryType type) const;

public:
    Context();
    Context(const Context &) = delete;
    Context(Context &&) = delete;
    ~Context();

    Context &operator=(const Context &) = delete;
    Context &operator=(Context &&) = delete;

    VkDeviceMemory allocate_memory(const VkMemoryRequirements &requirements, MemoryType type) const;
    const Vector<VkQueueFamilyProperties> &queue_families() const { return m_queue_families; }
};

} // namespace vull
