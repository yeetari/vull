#pragma once

#include <vull/support/Vector.hh>

#include <vulkan/vulkan_core.h>

namespace vull {

class Context {
    VkInstance m_instance{nullptr};
    VkPhysicalDevice m_physical_device{nullptr};
    VkDevice m_device{nullptr};
    Vector<VkMemoryType> m_memory_types;
    Vector<VkQueueFamilyProperties> m_queue_families;

public:
    Context();
    Context(const Context &) = delete;
    Context(Context &&) = delete;
    ~Context();

    Context &operator=(const Context &) = delete;
    Context &operator=(Context &&) = delete;
};

} // namespace vull
