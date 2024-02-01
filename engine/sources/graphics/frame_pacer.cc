#include <vull/graphics/frame_pacer.hh>

#include <vull/container/array.hh>
#include <vull/container/hash_map.hh>
#include <vull/container/vector.hh>
#include <vull/support/assert.hh>
#include <vull/support/string.hh>
#include <vull/support/string_view.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>
#include <vull/vulkan/command_buffer.hh>
#include <vull/vulkan/context.hh>
#include <vull/vulkan/fence.hh>
#include <vull/vulkan/image.hh>
#include <vull/vulkan/query_pool.hh>
#include <vull/vulkan/queue.hh>
#include <vull/vulkan/render_graph.hh>
#include <vull/vulkan/semaphore.hh>
#include <vull/vulkan/swapchain.hh>
#include <vull/vulkan/vulkan.hh>

namespace vull {

FramePacer::FramePacer(const vk::Swapchain &swapchain, uint32_t queue_length) : m_swapchain(swapchain) {
    VULL_ASSERT(queue_length > 0);
    m_frames.ensure_size(queue_length, swapchain.context());

    // Dummy first frame.
    auto &first_frame = m_frames.first();
    m_image_index = swapchain.acquire_image(*first_frame.acquire_semaphore());

    auto queue = swapchain.context().lock_queue(vk::QueueKind::Graphics);
    auto &cmd_buf = queue->request_cmd_buf();
    vkb::ImageMemoryBarrier2 swapchain_present_barrier{
        .sType = vkb::StructureType::ImageMemoryBarrier2,
        .oldLayout = vkb::ImageLayout::Undefined,
        .newLayout = vkb::ImageLayout::PresentSrcKHR,
        .image = *swapchain.image(m_image_index),
        .subresourceRange{
            .aspectMask = vkb::ImageAspect::Color,
            .levelCount = 1,
            .layerCount = 1,
        },
    };
    cmd_buf.image_barrier(swapchain_present_barrier);

    vkb::SemaphoreSubmitInfo wait_semaphore_info{
        .sType = vkb::StructureType::SemaphoreSubmitInfo,
        .semaphore = *first_frame.acquire_semaphore(),
    };
    vkb::SemaphoreSubmitInfo signal_semaphore_info{
        .sType = vkb::StructureType::SemaphoreSubmitInfo,
        .semaphore = *first_frame.present_semaphore(),
    };
    queue->submit(cmd_buf, nullptr, signal_semaphore_info, wait_semaphore_info);
    queue->wait_idle();
}

Frame &FramePacer::request_frame() {
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

Frame::Frame(const vk::Context &context)
    : m_fence(context, true), m_acquire_semaphore(context), m_present_semaphore(context) {
    context.set_object_name(m_fence, "Frame fence");
    context.set_object_name(m_acquire_semaphore, "Acquire semaphore");
    context.set_object_name(m_present_semaphore, "Present semaphore");
}

Frame::~Frame() = default;

HashMap<StringView, float> Frame::pass_times() {
    if (!m_render_graph) {
        return {};
    }

    auto &timestamp_pool = m_render_graph->timestamp_pool();
    if (!timestamp_pool) {
        return {};
    }

    Vector<uint64_t> timestamp_data(timestamp_pool.count());
    timestamp_pool.read_host(timestamp_data.span(), timestamp_pool.count());

    for (uint32_t i = 1; i < timestamp_data.size(); i++) {
        if (timestamp_data[i] == 0) {
            timestamp_data[i] = timestamp_data[i - 1];
        }
    }

    HashMap<StringView, float> times;
    for (uint32_t i = 0; i < m_render_graph->pass_count(); i++) {
        const vk::Pass &pass = m_render_graph->pass_order()[i];
        float time = timestamp_pool.context().timestamp_elapsed(timestamp_data[i], timestamp_data[i + 1]);
        times.set(pass.name(), time);
    }
    return times;
}

vk::RenderGraph &Frame::new_graph(vk::Context &context) {
    m_render_graph = vull::make_unique<vk::RenderGraph>(context);
    return *m_render_graph;
}

} // namespace vull
