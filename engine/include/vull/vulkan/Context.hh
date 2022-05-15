#pragma once

#include <vull/support/Vector.hh>
#include <vull/vulkan/ContextTable.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stdint.h>

namespace vull::vk {

enum class MemoryType {
    DeviceLocal,
    HostVisible,
    Staging,
};

class Context : public vkb::ContextTable {
    Vector<vkb::MemoryType> m_memory_types;
    Vector<vkb::QueueFamilyProperties> m_queue_families;

    uint32_t find_memory_type_index(const vkb::MemoryRequirements &requirements, MemoryType type) const;

public:
    Context();
    Context(const Context &) = delete;
    Context(Context &&) = delete;
    ~Context();

    Context &operator=(const Context &) = delete;
    Context &operator=(Context &&) = delete;

    vkb::DeviceMemory allocate_memory(const vkb::MemoryRequirements &requirements, MemoryType type) const;
    const Vector<vkb::QueueFamilyProperties> &queue_families() const { return m_queue_families; }
};

} // namespace vull::vk
