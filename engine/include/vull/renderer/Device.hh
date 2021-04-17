#pragma once

#include <vull/renderer/Buffer.hh>
#include <vull/renderer/Image.hh>
#include <vull/support/Optional.hh>
#include <vull/support/Vector.hh>

#include <vulkan/vulkan_core.h>

class Instance;

enum class BufferType {
    IndexBuffer,
    StorageBuffer,
    UniformBuffer,
    VertexBuffer,
};

enum class MemoryUsage {
    CpuToGpu,
    GpuOnly,
};

class Device {
    const VkPhysicalDevice m_physical;
    VkDevice m_device{nullptr};
    Vector<VkMemoryType> m_memory_types;
    Vector<VkQueueFamilyProperties> m_queue_families;

    Optional<std::uint32_t> find_memory_type(const VkMemoryRequirements &, VkMemoryPropertyFlags) const;

public:
    explicit Device(VkPhysicalDevice physical);
    Device(const Device &) = delete;
    Device(Device &&) = delete;
    ~Device();

    Device &operator=(const Device &) = delete;
    Device &operator=(Device &&) = delete;

    Buffer create_buffer(std::size_t size, BufferType type, MemoryUsage usage) const;
    Image create_image(const VkImageCreateInfo &image_ci, MemoryUsage usage) const;

    VkPhysicalDevice physical() const { return m_physical; }
    VkDevice operator*() const { return m_device; }
    const Vector<VkQueueFamilyProperties> &queue_families() const { return m_queue_families; }
};
