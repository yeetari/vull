#include <vull/vulkan/Queue.hh>

#include <vull/container/Vector.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Function.hh>
#include <vull/support/Span.hh>
#include <vull/support/Utility.hh>
#include <vull/vulkan/CommandBuffer.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/Vulkan.hh>

namespace vull::vk {

Queue::Queue(const Context &context, uint32_t family_index, uint32_t index)
    : m_context(context), m_family_index(family_index), m_index(index) {
    context.vkGetDeviceQueue(family_index, index, &m_queue);
    vkb::CommandPoolCreateInfo cmd_pool_ci{
        .sType = vkb::StructureType::CommandPoolCreateInfo,
        .flags = vkb::CommandPoolCreateFlags::Transient | vkb::CommandPoolCreateFlags::ResetCommandBuffer,
        .queueFamilyIndex = family_index,
    };
    VULL_ENSURE(context.vkCreateCommandPool(&cmd_pool_ci, &m_cmd_pool) == vkb::Result::Success);
}

Queue::Queue(Queue &&other) : m_context(other.m_context), m_family_index(other.m_family_index), m_index(other.m_index) {
    m_cmd_pool = vull::exchange(other.m_cmd_pool, nullptr);
    m_queue = vull::exchange(other.m_queue, nullptr);
    m_cmd_bufs = vull::move(other.m_cmd_bufs);
}

Queue::~Queue() {
    wait_idle();
    m_context.vkDestroyCommandPool(m_cmd_pool);
}

CommandBuffer &Queue::request_cmd_buf() {
    // Reset any completed command buffers.
    CommandBuffer *available_cmd_buf = nullptr;
    for (auto &cmd_buf : m_cmd_bufs) {
        if (!cmd_buf.m_in_flight) {
            // Command buffer already in recording state.
            available_cmd_buf = &cmd_buf;
            continue;
        }
        uint64_t value = 0;
        VULL_ENSURE(m_context.vkGetSemaphoreCounterValue(cmd_buf.completion_semaphore(), &value) ==
                    vkb::Result::Success);
        if (value == cmd_buf.completion_value()) {
            cmd_buf.reset();
            available_cmd_buf = &cmd_buf;
        }
    }

    // Reuse any completed command buffers.
    if (available_cmd_buf != nullptr) {
        return *available_cmd_buf;
    }

    // Else, allocate a new command buffer.
    vkb::CommandBufferAllocateInfo buffer_ai{
        .sType = vkb::StructureType::CommandBufferAllocateInfo,
        .commandPool = m_cmd_pool,
        .level = vkb::CommandBufferLevel::Primary,
        .commandBufferCount = 1,
    };
    vkb::CommandBuffer buffer;
    const auto allocation_result = m_context.vkAllocateCommandBuffers(&buffer_ai, &buffer);

    // If the allocation wasn't a success (out of either host or device memory), then we must wait for and reuse an
    // existing command buffer.
    if (allocation_result != vkb::Result::Success) {
        // The first command buffer allocation shouldn't fail.
        VULL_ENSURE(!m_cmd_bufs.empty());
        auto &cmd_buf = m_cmd_bufs.first();
        vkb::Semaphore wait_semaphore = cmd_buf.completion_semaphore();
        uint64_t wait_value = cmd_buf.completion_value();
        vkb::SemaphoreWaitInfo wait_info{
            .sType = vkb::StructureType::SemaphoreWaitInfo,
            .semaphoreCount = 1,
            .pSemaphores = &wait_semaphore,
            .pValues = &wait_value,
        };
        m_context.vkWaitSemaphores(&wait_info, ~0ull);
        cmd_buf.reset();
        return cmd_buf;
    }
    return m_cmd_bufs.emplace(m_context, buffer);
}

void Queue::immediate_submit(Function<void(CommandBuffer &)> callback) {
    auto &cmd_buf = request_cmd_buf();
    callback(cmd_buf);
    submit(cmd_buf, nullptr, {}, {});
    wait_idle();
}

void Queue::submit(CommandBuffer &cmd_buf, vkb::Fence signal_fence, Span<vkb::SemaphoreSubmitInfo> signal_semaphores,
                   Span<vkb::SemaphoreSubmitInfo> wait_semaphores) {
    m_context.vkEndCommandBuffer(*cmd_buf);
    cmd_buf.m_in_flight = true;

    // TODO(small-vector)
    Vector<vkb::SemaphoreSubmitInfo> signal_sems;
    signal_sems.ensure_capacity(static_cast<uint32_t>(signal_semaphores.size()) + 1);
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
        .waitSemaphoreInfoCount = static_cast<uint32_t>(wait_semaphores.size()),
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

} // namespace vull::vk
