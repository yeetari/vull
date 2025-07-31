#pragma once

#include <vull/container/vector.hh>
#include <vull/support/atomic.hh>
#include <vull/support/function.hh> // IWYU pragma: keep
#include <vull/support/span.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/tasklet/future.hh>
#include <vull/tasklet/mutex.hh>
#include <vull/vulkan/vulkan.hh>

#include <stdint.h>

namespace vull::vk {

class CommandBuffer;
class Context;

enum class QueueKind {
    Compute,
    Graphics,
    Transfer,
};

class Queue {
    friend Context;

private:
    const Context &m_context;
    const uint32_t m_family_index;
    Vector<vkb::Queue> m_queues;
    Vector<UniquePtr<CommandBuffer>> m_buffers;
    tasklet::Mutex m_buffers_mutex;
    tasklet::Mutex *m_submit_mutexes;
    Atomic<uint32_t> m_queue_index;
    Atomic<uint32_t> m_total_buffer_count;

    Queue(const Context &context, uint32_t family_index, uint32_t count);

public:
    Queue(const Queue &) = delete;
    Queue(Queue &&) = delete;
    ~Queue();

    Queue &operator=(const Queue &) = delete;
    Queue &operator=(Queue &&) = delete;

    UniquePtr<CommandBuffer> request_cmd_buf();
    vkb::Result present(const vkb::PresentInfoKHR &present_info);
    tasklet::Future<void> submit(UniquePtr<CommandBuffer> &&cmd_buf, Span<vkb::SemaphoreSubmitInfo> signal_semaphores,
                                 Span<vkb::SemaphoreSubmitInfo> wait_semaphores);
    void wait_idle();

    uint32_t family_index() const { return m_family_index; }
};

} // namespace vull::vk
