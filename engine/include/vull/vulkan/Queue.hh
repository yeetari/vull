#pragma once

#include <vull/support/Span.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stdint.h>

namespace vull {

class CommandBuffer;
class VkContext;

class Queue {
    const VkContext &m_context;
    vk::Queue m_queue;

public:
    Queue(const VkContext &context, uint32_t queue_family_index);

    void submit(const CommandBuffer &cmd_buf, vk::Fence signal_fence, Span<vk::SemaphoreSubmitInfo> signal_semaphores,
                Span<vk::SemaphoreSubmitInfo> wait_semaphores);
    void wait_idle();
};

} // namespace vull
