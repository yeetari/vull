#pragma once

#include <support/Vector.hh>

#include <vulkan/vulkan_core.h>

class Device {
    const VkPhysicalDevice m_physical;
    VkDevice m_device{nullptr};
    Vector<VkQueueFamilyProperties> m_queue_families;

public:
    explicit Device(VkPhysicalDevice physical);
    Device(const Device &) = delete;
    Device(Device &&) = delete;
    ~Device();

    Device &operator=(const Device &) = delete;
    Device &operator=(Device &&) = delete;

    VkDevice operator*() const { return m_device; }
    VkPhysicalDevice physical() const { return m_physical; }
    const Vector<VkQueueFamilyProperties> &queue_families() const { return m_queue_families; }
};
