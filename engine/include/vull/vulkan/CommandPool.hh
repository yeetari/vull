#pragma once

#include <vull/support/Vector.hh>
#include <vull/vulkan/CommandBuffer.hh> // IWYU pragma: keep
#include <vull/vulkan/Vulkan.hh>

#include <stdint.h>

namespace vull::vk {

class Context;

class CommandPool {
    const Context &m_context;
    vkb::CommandPool m_command_pool{nullptr};
    Vector<CommandBuffer> m_command_buffers;

public:
    CommandPool(const Context &context, uint32_t queue_family_index);
    CommandPool(const CommandPool &) = delete;
    CommandPool(CommandPool &&);
    ~CommandPool();

    CommandPool &operator=(const CommandPool &) = delete;
    CommandPool &operator=(CommandPool &&) = delete;

    CommandBuffer &request_cmd_buf();
};

} // namespace vull::vk
