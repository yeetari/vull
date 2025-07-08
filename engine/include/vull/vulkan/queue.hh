#pragma once

#include <vull/container/vector.hh>
#include <vull/support/atomic.hh>
#include <vull/support/function.hh> // IWYU pragma: keep
#include <vull/support/span.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>
#include <vull/tasklet/future.hh>
#include <vull/tasklet/mutex.hh>
#include <vull/vulkan/vulkan.hh>

#include <stdint.h>

namespace vull::vk {

class CommandBuffer;
class Context;
class QueueHandle;

enum class QueueKind {
    Compute,
    Graphics,
    Transfer,
};

class Queue {
    friend Context;
    friend QueueHandle;

private:
    const Context &m_context;
    const uint32_t m_family_index;
    const uint32_t m_index;
    vkb::Queue m_queue;
    Vector<UniquePtr<CommandBuffer>> m_buffers;
    tasklet::Mutex m_buffers_mutex;
    tasklet::Mutex m_submit_mutex;
    Atomic<uint32_t> m_total_buffer_count;

    Queue(const Context &context, uint32_t family_index, uint32_t index);

public:
    Queue(const Queue &) = delete;
    Queue(Queue &&) = delete;
    ~Queue();

    Queue &operator=(const Queue &) = delete;
    Queue &operator=(Queue &&) = delete;

    UniquePtr<CommandBuffer> request_cmd_buf();
    // TODO: Remove immediate_submit.
    void immediate_submit(Function<void(CommandBuffer &)> callback);
    void present(const vkb::PresentInfoKHR &present_info);
    tasklet::Future<void> submit(UniquePtr<CommandBuffer> &&cmd_buf, Span<vkb::SemaphoreSubmitInfo> signal_semaphores,
                                 Span<vkb::SemaphoreSubmitInfo> wait_semaphores);
    void wait_idle();

    vkb::Queue operator*() const { return m_queue; }
    uint32_t family_index() const { return m_family_index; }
    uint32_t index() const { return m_index; }
};

// TODO: Remove this.
class QueueHandle {
    friend Context;

private:
    Queue *m_queue;

    explicit QueueHandle(Queue &queue) : m_queue(&queue) {}

public:
    QueueHandle(const QueueHandle &) = delete;
    QueueHandle(QueueHandle &&other) : m_queue(vull::exchange(other.m_queue, nullptr)) {}
    ~QueueHandle() { release(); }

    QueueHandle &operator=(const QueueHandle &) = delete;
    QueueHandle &operator=(QueueHandle &&);

    void release();

    Queue &operator*() const { return *m_queue; }
    Queue *operator->() const { return m_queue; }
};

inline QueueHandle &QueueHandle::operator=(QueueHandle &&other) {
    QueueHandle moved(vull::move(other));
    vull::swap(m_queue, other.m_queue);
    return *this;
}

inline void QueueHandle::release() {
    if (m_queue != nullptr) {
        m_queue = nullptr;
    }
}

} // namespace vull::vk
