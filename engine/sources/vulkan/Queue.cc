#include <vull/vulkan/Queue.hh>

#include <vull/support/Function.hh>
#include <vull/support/Span.hh>
#include <vull/vulkan/CommandBuffer.hh>
#include <vull/vulkan/CommandPool.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/Vulkan.hh>

namespace vull {

Queue::Queue(const VkContext &context, uint32_t queue_family_index) : m_context(context) {
    context.vkGetDeviceQueue(queue_family_index, 0, &m_queue);
}

void Queue::immediate_submit(CommandPool &cmd_pool, Function<void(const CommandBuffer &)> callback) {
    cmd_pool.begin(vull::vk::CommandPoolResetFlags::None);
    auto cmd_buf = cmd_pool.request_cmd_buf();
    callback(cmd_buf);
    submit(cmd_buf, nullptr, {}, {});
    wait_idle();
}

void Queue::submit(const CommandBuffer &cmd_buf, vk::Fence signal_fence,
                   Span<vk::SemaphoreSubmitInfo> signal_semaphores, Span<vk::SemaphoreSubmitInfo> wait_semaphores) {
    m_context.vkEndCommandBuffer(*cmd_buf);

    vk::CommandBufferSubmitInfo cmd_buf_si{
        .sType = vk::StructureType::CommandBufferSubmitInfo,
        .commandBuffer = *cmd_buf,
    };
    vk::SubmitInfo2 submit_info{
        .sType = vk::StructureType::SubmitInfo2,
        .waitSemaphoreInfoCount = wait_semaphores.size(),
        .pWaitSemaphoreInfos = wait_semaphores.data(),
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &cmd_buf_si,
        .signalSemaphoreInfoCount = signal_semaphores.size(),
        .pSignalSemaphoreInfos = signal_semaphores.data(),
    };
    m_context.vkQueueSubmit2(m_queue, 1, &submit_info, signal_fence);
}

void Queue::wait_idle() {
    m_context.vkQueueWaitIdle(m_queue);
}

} // namespace vull
