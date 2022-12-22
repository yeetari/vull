#pragma once

#include <vull/support/UniquePtr.hh> // IWYU pragma: keep
#include <vull/support/Vector.hh>
#include <vull/vulkan/Allocation.hh>
#include <vull/vulkan/Buffer.hh>
#include <vull/vulkan/ContextTable.hh>
#include <vull/vulkan/Image.hh>
#include <vull/vulkan/MemoryUsage.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stddef.h>
#include <stdint.h>

namespace vull::vk {

class Allocator;
class Queue;

class Context : public vkb::ContextTable {
    vkb::PhysicalDeviceProperties m_properties{};
    vkb::PhysicalDeviceDescriptorBufferPropertiesEXT m_descriptor_buffer_properties{};
    vkb::PhysicalDeviceMemoryProperties m_memory_properties{};
    Vector<vkb::QueueFamilyProperties> m_queue_families;
    Vector<UniquePtr<Allocator>> m_allocators;
    Vector<Queue> m_queues;

    Allocator &allocator_for(const vkb::MemoryRequirements &, MemoryUsage);

public:
    explicit Context(bool enable_validation);
    Context(const Context &) = delete;
    Context(Context &&) = delete;
    ~Context();

    Context &operator=(const Context &) = delete;
    Context &operator=(Context &&) = delete;

    Allocation allocate_memory(const vkb::MemoryRequirements &requirements, MemoryUsage usage);
    Buffer create_buffer(vkb::DeviceSize size, vkb::BufferUsage usage, MemoryUsage memory_usage);
    Image create_image(const vkb::ImageCreateInfo &image_ci, MemoryUsage memory_usage);
    size_t descriptor_size(vkb::DescriptorType type) const;
    float timestamp_elapsed(uint64_t start, uint64_t end) const;
    Queue &graphics_queue();
};

} // namespace vull::vk
