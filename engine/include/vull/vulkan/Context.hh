#pragma once

#include <vull/support/Vector.hh>
#include <vull/vulkan/ContextTable.hh>

#include <vulkan/vulkan_core.h>

namespace vull {

class Context : public ContextTable {
    Vector<VkMemoryType> m_memory_types;
    Vector<VkQueueFamilyProperties> m_queue_families;

public:
    Context();
    Context(const Context &) = delete;
    Context(Context &&) = delete;
    ~Context();

    Context &operator=(const Context &) = delete;
    Context &operator=(Context &&) = delete;

    const Vector<VkQueueFamilyProperties> &queue_families() const { return m_queue_families; }
};

} // namespace vull
