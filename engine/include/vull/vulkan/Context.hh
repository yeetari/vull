#pragma once

#include <vull/support/UniquePtr.hh> // IWYU pragma: keep
#include <vull/support/Vector.hh>
#include <vull/vulkan/Allocation.hh>
#include <vull/vulkan/ContextTable.hh>
#include <vull/vulkan/MemoryUsage.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stdint.h>

namespace vull::vk {

class Allocator;

class Context : public vkb::ContextTable {
    vkb::PhysicalDeviceProperties m_properties{};
    vkb::PhysicalDeviceMemoryProperties m_memory_properties{};
    Vector<vkb::QueueFamilyProperties> m_queue_families;
    Vector<UniquePtr<Allocator>> m_allocators;

    Allocator &allocator_for(const vkb::MemoryRequirements &, MemoryUsage);

public:
    Context();
    Context(const Context &) = delete;
    Context(Context &&) = delete;
    ~Context();

    Context &operator=(const Context &) = delete;
    Context &operator=(Context &&) = delete;

    Allocation allocate_memory(const vkb::MemoryRequirements &requirements, MemoryUsage usage);
    Allocation bind_memory(vkb::Buffer buffer, MemoryUsage usage);
    Allocation bind_memory(vkb::Image image, MemoryUsage usage);
    float timestamp_elapsed(uint64_t start, uint64_t end) const;
    const Vector<vkb::QueueFamilyProperties> &queue_families() const { return m_queue_families; }
};

} // namespace vull::vk
