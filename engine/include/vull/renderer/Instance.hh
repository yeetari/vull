#pragma once

#include <vull/support/Span.hh>
#include <vull/support/Vector.hh>

#include <vulkan/vulkan_core.h>

class Instance {
    VkInstance m_instance{nullptr};
    Vector<VkPhysicalDevice> m_physical_devices;

public:
    explicit Instance(Span<const char *> extensions);
    Instance(const Instance &) = delete;
    Instance(Instance &&) = delete;
    ~Instance();

    Instance &operator=(const Instance &) = delete;
    Instance &operator=(Instance &&) = delete;

    VkInstance operator*() const { return m_instance; }
    const Vector<VkPhysicalDevice> &physical_devices() const { return m_physical_devices; }
};
