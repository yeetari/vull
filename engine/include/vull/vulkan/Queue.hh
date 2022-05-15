#pragma once

#include <vull/support/Function.hh> // IWYU pragma: keep
#include <vull/support/Span.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stdint.h>

namespace vull {

class CommandBuffer;
class CommandPool;
class VkContext;

class Queue {
    const VkContext &m_context;
    vkb::Queue m_queue;

public:
    Queue(const VkContext &context, uint32_t queue_family_index);

    void immediate_submit(CommandPool &cmd_pool, Function<void(const CommandBuffer &)> callback);
    void submit(const CommandBuffer &cmd_buf, vkb::Fence signal_fence, Span<vkb::SemaphoreSubmitInfo> signal_semaphores,
                Span<vkb::SemaphoreSubmitInfo> wait_semaphores);
    void wait_idle();
};

} // namespace vull
