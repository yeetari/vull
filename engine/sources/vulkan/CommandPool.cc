#include <vull/vulkan/CommandPool.hh>

#include <vull/support/Assert.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>
#include <vull/vulkan/CommandBuffer.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/Vulkan.hh>

namespace vull::vk {

CommandPool::CommandPool(const Context &context, uint32_t queue_family_index) : m_context(context) {
    vkb::CommandPoolCreateInfo pool_ci{
        .sType = vkb::StructureType::CommandPoolCreateInfo,
        .flags = vkb::CommandPoolCreateFlags::Transient | vkb::CommandPoolCreateFlags::ResetCommandBuffer,
        .queueFamilyIndex = queue_family_index,
    };
    VULL_ENSURE(context.vkCreateCommandPool(&pool_ci, &m_command_pool) == vkb::Result::Success);
}

CommandPool::CommandPool(CommandPool &&other) : m_context(other.m_context) {
    m_command_pool = exchange(other.m_command_pool, nullptr);
    m_command_buffers = move(other.m_command_buffers);
}

CommandPool::~CommandPool() {
    m_context.vkDestroyCommandPool(m_command_pool);
}

CommandBuffer &CommandPool::request_cmd_buf() {
    // Reuse any completed command buffers.
    for (auto &cmd_buf : m_command_buffers) {
        uint64_t value;
        VULL_ENSURE(m_context.vkGetSemaphoreCounterValue(cmd_buf.completion_semaphore(), &value) ==
                    vkb::Result::Success);
        if (value == cmd_buf.completion_value()) {
            cmd_buf.reset();
            return cmd_buf;
        }
    }

    // Else, allocate a new command buffer.
    vkb::CommandBufferAllocateInfo buffer_ai{
        .sType = vkb::StructureType::CommandBufferAllocateInfo,
        .commandPool = m_command_pool,
        .level = vkb::CommandBufferLevel::Primary,
        .commandBufferCount = 1,
    };
    vkb::CommandBuffer buffer;
    const auto allocation_result = m_context.vkAllocateCommandBuffers(&buffer_ai, &buffer);

    // If the allocation wasn't a success (out of either host or device memory), then we must wait for and reuse an
    // existing command buffer.
    if (allocation_result != vkb::Result::Success) {
        // The first command buffer allocation shouldn't fail.
        VULL_ENSURE(!m_command_buffers.empty());
        auto &cmd_buf = m_command_buffers.first();
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
    return m_command_buffers.emplace(m_context, buffer);
}

} // namespace vull::vk
