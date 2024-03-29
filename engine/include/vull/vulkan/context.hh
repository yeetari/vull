#pragma once

#include <vull/container/vector.hh>
#include <vull/support/string_view.hh>
#include <vull/vulkan/allocation.hh>
#include <vull/vulkan/buffer.hh>
#include <vull/vulkan/context_table.hh>
#include <vull/vulkan/image.hh>
#include <vull/vulkan/vulkan.hh>

#include <stddef.h>
#include <stdint.h>

namespace vull {

template <typename>
class UniquePtr;

} // namespace vull

namespace vull::vk {

class Allocator;
class Queue;
class QueueHandle;

enum class MemoryUsage;
enum class QueueKind;
enum class Sampler;

class Context : public vkb::ContextTable {
    vkb::DebugUtilsMessengerEXT m_debug_utils_messenger;
    vkb::PhysicalDeviceProperties m_properties{};
    vkb::PhysicalDeviceDescriptorBufferPropertiesEXT m_descriptor_buffer_properties{};
    vkb::PhysicalDeviceMemoryProperties m_memory_properties{};
    Vector<UniquePtr<Allocator>> m_allocators;
    Vector<UniquePtr<Queue>> m_queues;
    Vector<Queue &> m_compute_queues;
    Vector<Queue &> m_graphics_queues;
    Vector<Queue &> m_transfer_queues;
    vkb::Sampler m_nearest_sampler;
    vkb::Sampler m_linear_sampler;
    vkb::Sampler m_depth_reduce_sampler;
    vkb::Sampler m_shadow_sampler;

    Allocator &allocator_for(const vkb::MemoryRequirements &, MemoryUsage);
    Vector<Queue &> &queue_list_for(QueueKind kind);

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
    QueueHandle lock_queue(QueueKind kind);

    template <typename T>
    void set_object_name(const T &object, StringView name) const;

    size_t descriptor_size(vkb::DescriptorType type) const;
    vkb::Sampler get_sampler(Sampler sampler) const;
    float timestamp_elapsed(uint64_t start, uint64_t end) const;
    const vkb::PhysicalDeviceProperties &properties() const { return m_properties; }
    const Vector<UniquePtr<Allocator>> &allocators() const { return m_allocators; }
};

} // namespace vull::vk
