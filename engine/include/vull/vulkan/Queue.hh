#pragma once

#include <vull/support/Function.hh> // IWYU pragma: keep
#include <vull/support/Span.hh>
#include <vull/support/Vector.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stdint.h>

namespace vull::vk {

class CommandBuffer;
class Context;

class Queue {
    friend Context;

private:
    const Context &m_context;
    const uint32_t m_family_index;
    vkb::CommandPool m_cmd_pool;
    vkb::Queue m_queue;
    Vector<CommandBuffer> m_cmd_bufs;

    Queue(const Context &context, uint32_t family_index);

public:
    Queue(const Queue &) = delete;
    Queue(Queue &&);
    ~Queue();

    Queue &operator=(const Queue &) = delete;
    Queue &operator=(Queue &&) = delete;

    CommandBuffer &request_cmd_buf();
    void immediate_submit(Function<void(CommandBuffer &)> callback);
    void submit(const CommandBuffer &cmd_buf, vkb::Fence signal_fence, Span<vkb::SemaphoreSubmitInfo> signal_semaphores,
                Span<vkb::SemaphoreSubmitInfo> wait_semaphores);
    void wait_idle();

    vkb::Queue operator*() const { return m_queue; }
    uint32_t family_index() const { return m_family_index; }
};

} // namespace vull::vk
