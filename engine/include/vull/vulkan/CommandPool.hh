#pragma once

#include <vull/support/Vector.hh>
#include <vull/vulkan/CommandBuffer.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stdint.h>

namespace vull {

class VkContext;

class CommandPool {
    const VkContext &m_context;
    vk::CommandPool m_command_pool{nullptr};
    Vector<vk::CommandBuffer> m_command_buffers;
    uint32_t m_head{0};

public:
    CommandPool(const VkContext &context, uint32_t queue_family_index);
    CommandPool(const CommandPool &) = delete;
    CommandPool(CommandPool &&);
    ~CommandPool();

    CommandPool &operator=(const CommandPool &) = delete;
    CommandPool &operator=(CommandPool &&) = delete;

    void begin(vk::CommandPoolResetFlags flags);
    CommandBuffer request_cmd_buf();
};

} // namespace vull
