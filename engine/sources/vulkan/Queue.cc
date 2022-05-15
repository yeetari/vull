#include <vull/vulkan/Queue.hh>

#include <vull/support/Function.hh>
#include <vull/support/Span.hh>
#include <vull/support/Vector.hh>
#include <vull/vulkan/CommandBuffer.hh>
#include <vull/vulkan/CommandPool.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/Vulkan.hh>

namespace vull {

Queue::Queue(const VkContext &context, uint32_t queue_family_index) : m_context(context) {
    context.vkGetDeviceQueue(queue_family_index, 0, &m_queue);
}

void Queue::immediate_submit(CommandPool &cmd_pool, Function<void(const CommandBuffer &)> callback) {
    const auto &cmd_buf = cmd_pool.request_cmd_buf();
    callback(cmd_buf);
    submit(cmd_buf, nullptr, {}, {});
    wait_idle();
}

void Queue::submit(const CommandBuffer &cmd_buf, vkb::Fence signal_fence,
                   Span<vkb::SemaphoreSubmitInfo> signal_semaphores, Span<vkb::SemaphoreSubmitInfo> wait_semaphores) {
    m_context.vkEndCommandBuffer(*cmd_buf);

    // TODO(small-vector)
    Vector<vkb::SemaphoreSubmitInfo> signal_sems;
    signal_sems.ensure_capacity(signal_semaphores.size() + 1);
    signal_sems.extend(signal_semaphores);
    signal_sems.push({
        .sType = vkb::StructureType::SemaphoreSubmitInfo,
        .semaphore = cmd_buf.completion_semaphore(),
        .value = cmd_buf.completion_value(),
    });

    vkb::CommandBufferSubmitInfo cmd_buf_si{
        .sType = vkb::StructureType::CommandBufferSubmitInfo,
        .commandBuffer = *cmd_buf,
    };
    vkb::SubmitInfo2 submit_info{
        .sType = vkb::StructureType::SubmitInfo2,
        .waitSemaphoreInfoCount = wait_semaphores.size(),
        .pWaitSemaphoreInfos = wait_semaphores.data(),
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &cmd_buf_si,
        .signalSemaphoreInfoCount = signal_sems.size(),
        .pSignalSemaphoreInfos = signal_sems.data(),
    };
    m_context.vkQueueSubmit2(m_queue, 1, &submit_info, signal_fence);
}

void Queue::wait_idle() {
    m_context.vkQueueWaitIdle(m_queue);
}

} // namespace vull
