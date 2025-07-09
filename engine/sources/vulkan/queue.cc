#include <vull/vulkan/queue.hh>

#include <vull/container/vector.hh>
#include <vull/core/log.hh>
#include <vull/support/assert.hh>
#include <vull/support/atomic.hh>
#include <vull/support/function.hh>
#include <vull/support/scoped_lock.hh>
#include <vull/support/span.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>
#include <vull/tasklet/functions.hh>
#include <vull/tasklet/future.hh>
#include <vull/tasklet/io.hh>
#include <vull/tasklet/mutex.hh>
#include <vull/vulkan/command_buffer.hh>
#include <vull/vulkan/context.hh>
#include <vull/vulkan/fence.hh>
#include <vull/vulkan/vulkan.hh>

#include <stdint.h>

namespace vull::vk {

Queue::Queue(const Context &context, uint32_t family_index, uint32_t count)
    : m_context(context), m_family_index(family_index) {
    m_queues.ensure_size(count);
    for (uint32_t index = 0; index < count; index++) {
        context.vkGetDeviceQueue(family_index, index, &m_queues[index]);
    }
    m_submit_mutexes = new tasklet::Mutex[count];
}

Queue::~Queue() {
    wait_idle();
    delete[] m_submit_mutexes;
}

UniquePtr<CommandBuffer> Queue::request_cmd_buf() {
    // Try to use an available buffer first.
    ScopedLock lock(m_buffers_mutex);
    if (!m_buffers.empty()) {
        return m_buffers.take_last();
    }
    lock.unlock();

    // Otherwise allocate a new one.
    const auto total_count = m_total_buffer_count.fetch_add(1, vull::memory_order_relaxed) + 1;
    if (total_count % 10 == 0) {
        vull::trace("[vulkan] Reached {} command buffers for queue family {}", total_count, m_family_index);
    }

    // Create a pool first.
    vkb::CommandPoolCreateInfo pool_ci{
        .sType = vkb::StructureType::CommandPoolCreateInfo,
        .flags = vkb::CommandPoolCreateFlags::Transient,
        .queueFamilyIndex = m_family_index,
    };
    vkb::CommandPool pool;
    VULL_ENSURE(m_context.vkCreateCommandPool(&pool_ci, &pool) == vkb::Result::Success);

    // Then allocate a buffer.
    vkb::CommandBufferAllocateInfo buffer_ai{
        .sType = vkb::StructureType::CommandBufferAllocateInfo,
        .commandPool = pool,
        .level = vkb::CommandBufferLevel::Primary,
        .commandBufferCount = 1,
    };
    vkb::CommandBuffer buffer;
    VULL_ENSURE(m_context.vkAllocateCommandBuffers(&buffer_ai, &buffer) == vkb::Result::Success);
    auto cmd_buf = vull::make_unique<CommandBuffer>(m_context, pool, buffer);
    cmd_buf->reset();
    return cmd_buf;
}

void Queue::immediate_submit(Function<void(CommandBuffer &)> callback) {
    auto cmd_buf = request_cmd_buf();
    callback(*cmd_buf);
    submit(vull::move(cmd_buf), {}, {});
    wait_idle();
}

void Queue::present(const vkb::PresentInfoKHR &present_info) {
    ScopedLock lock(m_submit_mutexes[0]);
    m_context.vkQueuePresentKHR(m_queues[0], &present_info);
}

tasklet::Future<void> Queue::submit(UniquePtr<CommandBuffer> &&cmd_buf,
                                    Span<vkb::SemaphoreSubmitInfo> signal_semaphores,
                                    Span<vkb::SemaphoreSubmitInfo> wait_semaphores) {
    m_context.vkEndCommandBuffer(**cmd_buf);

    // Pick a vkb::Queue to use.
    const auto queue_index = m_queue_index.fetch_add(1, vull::memory_order_relaxed) % m_queues.size();

    // Submit the command buffer to the queue.
    vkb::CommandBufferSubmitInfo cmd_buf_si{
        .sType = vkb::StructureType::CommandBufferSubmitInfo,
        .commandBuffer = **cmd_buf,
    };
    vkb::SubmitInfo2 submit_info{
        .sType = vkb::StructureType::SubmitInfo2,
        .waitSemaphoreInfoCount = static_cast<uint32_t>(wait_semaphores.size()),
        .pWaitSemaphoreInfos = wait_semaphores.data(),
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &cmd_buf_si,
        .signalSemaphoreInfoCount = static_cast<uint32_t>(signal_semaphores.size()),
        .pSignalSemaphoreInfos = signal_semaphores.data(),
    };
    ScopedLock lock(m_submit_mutexes[queue_index]);
    m_context.vkQueueSubmit2(m_queues[queue_index], 1, &submit_info, *cmd_buf->completion_fence());
    lock.unlock();

    return tasklet::submit_io_request<tasklet::WaitVkFenceRequest>(cmd_buf->completion_fence())
        .and_then([this, cmd_buf = vull::move(cmd_buf)](tasklet::IoResult) mutable {
        cmd_buf->reset();
        ScopedLock lock(m_buffers_mutex);
        m_buffers.push(vull::move(cmd_buf));
    });
}

void Queue::wait_idle() {
    for (auto *queue : m_queues) {
        m_context.vkQueueWaitIdle(queue);
    }
}

} // namespace vull::vk
