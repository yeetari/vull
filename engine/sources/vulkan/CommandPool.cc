#include <vull/vulkan/CommandPool.hh>

#include <vull/support/Assert.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>
#include <vull/vulkan/CommandBuffer.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/Vulkan.hh>

namespace vull {

CommandPool::CommandPool(const VkContext &context, uint32_t queue_family_index) : m_context(context) {
    vk::CommandPoolCreateInfo pool_ci{
        .sType = vk::StructureType::CommandPoolCreateInfo,
        .flags = vk::CommandPoolCreateFlags::Transient,
        .queueFamilyIndex = queue_family_index,
    };
    VULL_ENSURE(context.vkCreateCommandPool(&pool_ci, &m_command_pool) == vk::Result::Success);
}

CommandPool::CommandPool(CommandPool &&other) : m_context(other.m_context) {
    m_command_pool = exchange(other.m_command_pool, nullptr);
    m_command_buffers = move(other.m_command_buffers);
}

CommandPool::~CommandPool() {
    m_context.vkDestroyCommandPool(m_command_pool);
}

void CommandPool::begin(vk::CommandPoolResetFlags flags) {
    if (exchange(m_head, 0u) != 0u) {
        m_context.vkResetCommandPool(m_command_pool, flags);
    }
}

CommandBuffer CommandPool::request_cmd_buf() {
    if (m_head < m_command_buffers.size()) {
        return {m_context, m_command_buffers[m_head++]};
    }
    m_command_buffers.ensure_size(++m_head);
    vk::CommandBufferAllocateInfo buffer_ai{
        .sType = vk::StructureType::CommandBufferAllocateInfo,
        .commandPool = m_command_pool,
        .level = vk::CommandBufferLevel::Primary,
        .commandBufferCount = 1,
    };
    VULL_ENSURE(m_context.vkAllocateCommandBuffers(&buffer_ai, &m_command_buffers.last()) == vk::Result::Success);
    return {m_context, m_command_buffers.last()};
}

} // namespace vull
