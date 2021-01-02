#pragma once

#include <vull/support/Vector.hh>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

class Instance;

class Device {
    const VkPhysicalDevice m_physical;
    VmaAllocator m_allocator{nullptr};
    VkDevice m_device{nullptr};
    Vector<VkQueueFamilyProperties> m_queue_families;

public:
    Device(const Instance &instance, VkPhysicalDevice physical);
    Device(const Device &) = delete;
    Device(Device &&) = delete;
    ~Device();

    Device &operator=(const Device &) = delete;
    Device &operator=(Device &&) = delete;

    VkPhysicalDevice physical() const { return m_physical; }
    VmaAllocator allocator() const { return m_allocator; }
    VkDevice operator*() const { return m_device; }
    const Vector<VkQueueFamilyProperties> &queue_families() const { return m_queue_families; }
};
