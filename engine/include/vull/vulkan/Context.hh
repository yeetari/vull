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

class Context : public vk::ContextTable {
    Vector<vk::MemoryType> m_memory_types;
    Vector<vk::QueueFamilyProperties> m_queue_families;

    uint32_t find_memory_type_index(const vk::MemoryRequirements &requirements, MemoryType type) const;

public:
    Context();
    Context(const Context &) = delete;
    Context(Context &&) = delete;
    ~Context();

    Context &operator=(const Context &) = delete;
    Context &operator=(Context &&) = delete;

    vk::DeviceMemory allocate_memory(const vk::MemoryRequirements &requirements, MemoryType type) const;
    const Vector<vk::QueueFamilyProperties> &queue_families() const { return m_queue_families; }
};

} // namespace vull
