#include <vull/graphics/FramePacer.hh>

#include <vull/graphics/Frame.hh>
#include <vull/support/Array.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>
#include <vull/vulkan/CommandBuffer.hh>
#include <vull/vulkan/CommandPool.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/Fence.hh>
#include <vull/vulkan/Semaphore.hh>
#include <vull/vulkan/Swapchain.hh>
#include <vull/vulkan/Vulkan.hh>

namespace vull {

FramePacer::FramePacer(const vk::Swapchain &swapchain, uint32_t queue_length) : m_swapchain(swapchain) {
    VULL_ASSERT(queue_length > 0);
    m_frames.ensure_size(queue_length, swapchain.context());

    // Dummy first frame.
    // TODO: This code is ugly.
    auto &first_frame = m_frames.first();
    m_image_index = swapchain.acquire_image(*first_frame.acquire_semaphore());

    vk::CommandPool cmd_pool(swapchain.context(), 0);
    auto &cmd_buf = cmd_pool.request_cmd_buf();
    vkb::ImageMemoryBarrier2 swapchain_present_barrier{
        .sType = vkb::StructureType::ImageMemoryBarrier2,
        .oldLayout = vkb::ImageLayout::Undefined,
        .newLayout = vkb::ImageLayout::PresentSrcKHR,
        .image = swapchain.image(m_image_index),
        .subresourceRange{
            .aspectMask = vkb::ImageAspect::Color,
            .levelCount = 1,
            .layerCount = 1,
        },
    };
    cmd_buf.image_barrier(swapchain_present_barrier);
    swapchain.context().vkEndCommandBuffer(*cmd_buf);

    vkb::CommandBufferSubmitInfo cmd_buf_si{
        .sType = vkb::StructureType::CommandBufferSubmitInfo,
        .commandBuffer = *cmd_buf,
    };
    vkb::SemaphoreSubmitInfo wait_semaphore_info{
        .sType = vkb::StructureType::SemaphoreSubmitInfo,
        .semaphore = *first_frame.acquire_semaphore(),
    };
    Array signal_semaphore_infos{
        vkb::SemaphoreSubmitInfo{
            .sType = vkb::StructureType::SemaphoreSubmitInfo,
            .semaphore = *first_frame.present_semaphore(),
        },
        vkb::SemaphoreSubmitInfo{
            .sType = vkb::StructureType::SemaphoreSubmitInfo,
            .semaphore = cmd_buf.completion_semaphore(),
            .value = cmd_buf.completion_value(),
        },
    };
    vkb::SubmitInfo2 submit_info{
        .sType = vkb::StructureType::SubmitInfo2,
        .waitSemaphoreInfoCount = 1,
        .pWaitSemaphoreInfos = &wait_semaphore_info,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &cmd_buf_si,
        .signalSemaphoreInfoCount = signal_semaphore_infos.size(),
        .pSignalSemaphoreInfos = signal_semaphore_infos.data(),
    };
    swapchain.context().vkQueueSubmit2(swapchain.present_queue(), 1, &submit_info, nullptr);
    swapchain.context().vkQueueWaitIdle(swapchain.present_queue());
}

Frame &FramePacer::next_frame() {
    // Present current frame.
    Array present_wait_semaphores{*m_frames[m_frame_index].present_semaphore()};
    m_swapchain.present(m_image_index, present_wait_semaphores.span());

    // Advance frame index.
    m_frame_index = (m_frame_index + 1) % m_frames.size();
    auto &frame = m_frames[m_frame_index];

    // Acquire swapchain image for next frame.
    m_image_index = m_swapchain.acquire_image(*frame.acquire_semaphore());

    // Wait on fence if host running ahead.
    const auto &wait_fence = frame.fence();
    wait_fence.wait();
    wait_fence.reset();
    return frame;
}

} // namespace vull