#pragma once

#include <vull/container/vector.hh>
#include <vull/support/function.hh> // IWYU pragma: keep
#include <vull/support/span.hh>
#include <vull/support/utility.hh>
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
    vkb::CommandPool m_cmd_pool;
    vkb::Queue m_queue;
    Vector<CommandBuffer> m_cmd_bufs;
    tasklet::Mutex m_mutex;

    Queue(const Context &context, uint32_t family_index, uint32_t index);

public:
    Queue(const Queue &) = delete;
    Queue(Queue &&);
    ~Queue();

    Queue &operator=(const Queue &) = delete;
    Queue &operator=(Queue &&) = delete;

    CommandBuffer &request_cmd_buf();
    void immediate_submit(Function<void(CommandBuffer &)> callback);
    void submit(CommandBuffer &cmd_buf, vkb::Fence signal_fence, Span<vkb::SemaphoreSubmitInfo> signal_semaphores,
                Span<vkb::SemaphoreSubmitInfo> wait_semaphores);
    void wait_idle();

    vkb::Queue operator*() const { return m_queue; }
    uint32_t family_index() const { return m_family_index; }
    uint32_t index() const { return m_index; }
};

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
        m_queue->m_mutex.unlock();
        m_queue = nullptr;
    }
}

} // namespace vull::vk
