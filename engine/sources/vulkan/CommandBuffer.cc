#include <vull/vulkan/CommandBuffer.hh>

#include <vull/support/Assert.hh>
#include <vull/support/Span.hh>
#include <vull/support/Utility.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/Vulkan.hh>

namespace vull::vk {

CommandBuffer::CommandBuffer(const Context &context, vkb::CommandBuffer cmd_buf)
    : m_context(context), m_cmd_buf(cmd_buf) {
    vkb::SemaphoreTypeCreateInfo timeline_ci{
        .sType = vkb::StructureType::SemaphoreTypeCreateInfo,
        .semaphoreType = vkb::SemaphoreType::Timeline,
        .initialValue = m_completion_value++,
    };
    vkb::SemaphoreCreateInfo semaphore_ci{
        .sType = vkb::StructureType::SemaphoreCreateInfo,
        .pNext = &timeline_ci,
    };
    VULL_ENSURE(m_context.vkCreateSemaphore(&semaphore_ci, &m_completion_semaphore) == vkb::Result::Success);

    vkb::CommandBufferBeginInfo cmd_buf_bi{
        .sType = vkb::StructureType::CommandBufferBeginInfo,
        .flags = vkb::CommandBufferUsage::OneTimeSubmit,
    };
    context.vkBeginCommandBuffer(cmd_buf, &cmd_buf_bi);
}

CommandBuffer::CommandBuffer(CommandBuffer &&other) : m_context(other.m_context), m_cmd_buf(other.m_cmd_buf) {
    m_completion_semaphore = vull::exchange(other.m_completion_semaphore, nullptr);
    m_completion_value = vull::exchange(other.m_completion_value, 0u);
}

CommandBuffer::~CommandBuffer() {
    [[maybe_unused]] uint64_t value;
    VULL_ASSERT(m_context.vkGetSemaphoreCounterValue(m_completion_semaphore, &value) == vkb::Result::Success);
    VULL_ASSERT(value == m_completion_value);
    m_context.vkDestroySemaphore(m_completion_semaphore);
}

void CommandBuffer::reset() {
    // Reset the semaphore to an uncompleted state and the command buffer back to a fresh recording state. Since the
    // command pool was created with the RESET_COMMAND_BUFFER flag, the reset is implicitly performed by
    // vkBeginCommandBuffer.
    vkb::SemaphoreSignalInfo signal_info{
        .sType = vkb::StructureType::SemaphoreSignalInfo,
        .semaphore = m_completion_semaphore,
        .value = (++m_completion_value)++,
    };
    VULL_ENSURE(m_context.vkSignalSemaphore(&signal_info) == vkb::Result::Success);
    vkb::CommandBufferBeginInfo cmd_buf_bi{
        .sType = vkb::StructureType::CommandBufferBeginInfo,
        .flags = vkb::CommandBufferUsage::OneTimeSubmit,
    };
    m_context.vkBeginCommandBuffer(m_cmd_buf, &cmd_buf_bi);
}

void CommandBuffer::begin_rendering(const vkb::RenderingInfo &rendering_info) const {
    m_context.vkCmdBeginRendering(m_cmd_buf, &rendering_info);
}

void CommandBuffer::end_rendering() const {
    m_context.vkCmdEndRendering(m_cmd_buf);
}

void CommandBuffer::bind_descriptor_sets(vkb::PipelineBindPoint bind_point, vkb::PipelineLayout layout,
                                         Span<vkb::DescriptorSet> descriptor_sets) const {
    m_context.vkCmdBindDescriptorSets(m_cmd_buf, bind_point, layout, 0, descriptor_sets.size(), descriptor_sets.data(),
                                      0, nullptr);
}

void CommandBuffer::bind_index_buffer(vkb::Buffer buffer, vkb::IndexType index_type) const {
    m_context.vkCmdBindIndexBuffer(m_cmd_buf, buffer, 0, index_type);
}

void CommandBuffer::bind_pipeline(vkb::PipelineBindPoint bind_point, vkb::Pipeline pipeline) const {
    m_context.vkCmdBindPipeline(m_cmd_buf, bind_point, pipeline);
}

void CommandBuffer::bind_vertex_buffer(vkb::Buffer buffer) const {
    const vkb::DeviceSize offset = 0;
    m_context.vkCmdBindVertexBuffers(m_cmd_buf, 0, 1, &buffer, &offset);
}

void CommandBuffer::copy_buffer(vkb::Buffer src, vkb::Buffer dst, Span<vkb::BufferCopy> regions) const {
    m_context.vkCmdCopyBuffer(m_cmd_buf, src, dst, regions.size(), regions.data());
}

void CommandBuffer::copy_buffer_to_image(vkb::Buffer src, vkb::Image dst, vkb::ImageLayout dst_layout,
                                         Span<vkb::BufferImageCopy> regions) const {
    m_context.vkCmdCopyBufferToImage(m_cmd_buf, src, dst, dst_layout, regions.size(), regions.data());
}

void CommandBuffer::push_constants(vkb::PipelineLayout layout, vkb::ShaderStage stage, uint32_t size,
                                   const void *data) const {
    m_context.vkCmdPushConstants(m_cmd_buf, layout, stage, 0, size, data);
}

void CommandBuffer::dispatch(uint32_t x, uint32_t y, uint32_t z) const {
    m_context.vkCmdDispatch(m_cmd_buf, x, y, z);
}

void CommandBuffer::draw(uint32_t vertex_count, uint32_t instance_count) const {
    m_context.vkCmdDraw(m_cmd_buf, vertex_count, instance_count, 0, 0);
}

void CommandBuffer::draw_indexed(uint32_t index_count, uint32_t instance_count) const {
    m_context.vkCmdDrawIndexed(m_cmd_buf, index_count, instance_count, 0, 0, 0);
}

void CommandBuffer::image_barrier(const vkb::ImageMemoryBarrier2 &barrier) const {
    pipeline_barrier({
        .sType = vkb::StructureType::DependencyInfo,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
    });
}

void CommandBuffer::pipeline_barrier(const vkb::DependencyInfo &dependency_info) const {
    m_context.vkCmdPipelineBarrier2(m_cmd_buf, &dependency_info);
}

void CommandBuffer::reset_query_pool(vkb::QueryPool query_pool, uint32_t query_count) const {
    m_context.vkCmdResetQueryPool(m_cmd_buf, query_pool, 0, query_count);
}

void CommandBuffer::write_timestamp(vkb::PipelineStage2 stage, vkb::QueryPool query_pool, uint32_t query) const {
    // TODO: Neither amdvlk nor radv seem to handle None.
    if (stage == vkb::PipelineStage2::None) {
        stage = vkb::PipelineStage2::TopOfPipe;
    }
    m_context.vkCmdWriteTimestamp2(m_cmd_buf, stage, query_pool, query);
}

} // namespace vull::vk
