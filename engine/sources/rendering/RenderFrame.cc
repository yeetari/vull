#include <vull/rendering/RenderFrame.hh>

#include <vull/vulkan/Device.hh>

#include <vulkan/vulkan_core.h>

RenderFrame::RenderFrame(const Device &device, std::uint32_t stage_count) : m_device(device) {
    // TODO: Use dedicated transfer queue for transfer buffer.
    VkCommandPoolCreateInfo command_pool_ci{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = 0, // TODO: Don't hardcode.
    };
    ENSURE(vkCreateCommandPool(*device, &command_pool_ci, nullptr, &m_command_pool) == VK_SUCCESS);

    m_command_buffers.resize(stage_count + 1);
    VkCommandBufferAllocateInfo command_buffer_ai{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = m_command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = m_command_buffers.size(),
    };
    ENSURE(vkAllocateCommandBuffers(*device, &command_buffer_ai, m_command_buffers.data()) == VK_SUCCESS);
    m_transfer_buffer = m_command_buffers[stage_count];

    VkDescriptorPoolCreateInfo descriptor_pool_ci{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = stage_count,
        .poolSizeCount = 0,
        .pPoolSizes = nullptr,
    };
    ENSURE(vkCreateDescriptorPool(*device, &descriptor_pool_ci, nullptr, &m_descriptor_pool) == VK_SUCCESS);

    VkDescriptorSetAllocateInfo descriptor_set_ai{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = m_descriptor_pool,
        .descriptorSetCount = stage_count,
        .pSetLayouts = nullptr,
    };
    ENSURE(vkAllocateDescriptorSets(*device, &descriptor_set_ai, m_descriptor_sets.data()) == VK_SUCCESS);
}

RenderFrame::~RenderFrame() {
    vkDestroyDescriptorPool(*m_device, m_descriptor_pool, nullptr);
    vkDestroyCommandPool(*m_device, m_command_pool, nullptr);
}

void RenderFrame::enqueue_deletion(const StagingBuffer &staging_buffer) {
    m_staging_buffer_deletion_queue.push(staging_buffer);
}

void RenderFrame::execute_pending_deletions() {
    while (!m_staging_buffer_deletion_queue.empty()) {
        auto buffer = m_staging_buffer_deletion_queue.pop();
        vkDestroyBuffer(*m_device, buffer.buffer(), nullptr);
        vkFreeMemory(*m_device, buffer.memory(), nullptr);
    }
}

void RenderFrame::reset_pool() {
    vkResetCommandPool(*m_device, m_command_pool, 0);
}
