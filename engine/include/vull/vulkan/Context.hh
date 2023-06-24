#pragma once

#include <vull/container/Vector.hh>
#include <vull/support/StringView.hh>
#include <vull/tasklet/Mutex.hh>
#include <vull/vulkan/Allocation.hh>
#include <vull/vulkan/Buffer.hh>
#include <vull/vulkan/ContextTable.hh>
#include <vull/vulkan/Image.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stddef.h>
#include <stdint.h>

namespace vull {

template <typename>
class UniquePtr;

} // namespace vull

namespace vull::vk {

class Allocator;
class Queue;

enum class MemoryUsage;
enum class Sampler;

class Context : public vkb::ContextTable {
    vkb::DebugUtilsMessengerEXT m_debug_utils_messenger;
    vkb::PhysicalDeviceProperties m_properties{};
    vkb::PhysicalDeviceDescriptorBufferPropertiesEXT m_descriptor_buffer_properties{};
    vkb::PhysicalDeviceMemoryProperties m_memory_properties{};
    Vector<vkb::QueueFamilyProperties> m_queue_families;
    Vector<UniquePtr<Allocator>> m_allocators;
    vkb::Sampler m_nearest_sampler;
    vkb::Sampler m_linear_sampler;
    vkb::Sampler m_depth_reduce_sampler;
    vkb::Sampler m_shadow_sampler;

    Vector<UniquePtr<Queue>> m_queues;
    Mutex m_queues_mutex;

    Allocator &allocator_for(const vkb::MemoryRequirements &, MemoryUsage);
    template <vkb::ObjectType ObjectType>
    void set_object_name(const void *object, StringView name) const;

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

    template <typename T>
    void set_object_name(const T &object, StringView name) const;

    size_t descriptor_size(vkb::DescriptorType type) const;
    vkb::Sampler get_sampler(Sampler sampler) const;
    float timestamp_elapsed(uint64_t start, uint64_t end) const;
    Queue &graphics_queue();
    const Vector<UniquePtr<Allocator>> &allocators() const { return m_allocators; }
};

} // namespace vull::vk
