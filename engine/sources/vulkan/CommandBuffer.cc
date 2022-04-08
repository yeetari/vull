#include <vull/vulkan/CommandBuffer.hh>

#include <vull/support/Span.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/Vulkan.hh>

namespace vull {

CommandBuffer::CommandBuffer(const VkContext &context, vk::CommandBuffer cmd_buf)
    : m_context(context), m_cmd_buf(cmd_buf) {
    vk::CommandBufferBeginInfo cmd_buf_bi{
        .sType = vk::StructureType::CommandBufferBeginInfo,
        .flags = vk::CommandBufferUsage::OneTimeSubmit,
    };
    context.vkBeginCommandBuffer(cmd_buf, &cmd_buf_bi);
}

void CommandBuffer::begin_rendering(const vk::RenderingInfo &rendering_info) const {
    m_context.vkCmdBeginRendering(m_cmd_buf, &rendering_info);
}

void CommandBuffer::end_rendering() const {
    m_context.vkCmdEndRendering(m_cmd_buf);
}

void CommandBuffer::bind_descriptor_sets(vk::PipelineBindPoint bind_point, vk::PipelineLayout layout,
                                         Span<vk::DescriptorSet> descriptor_sets) const {
    m_context.vkCmdBindDescriptorSets(m_cmd_buf, bind_point, layout, 0, descriptor_sets.size(), descriptor_sets.data(),
                                      0, nullptr);
}

void CommandBuffer::bind_index_buffer(vk::Buffer buffer, vk::IndexType index_type) const {
    m_context.vkCmdBindIndexBuffer(m_cmd_buf, buffer, 0, index_type);
}

void CommandBuffer::bind_pipeline(vk::PipelineBindPoint bind_point, vk::Pipeline pipeline) const {
    m_context.vkCmdBindPipeline(m_cmd_buf, bind_point, pipeline);
}

void CommandBuffer::bind_vertex_buffer(vk::Buffer buffer) const {
    const vk::DeviceSize offset = 0;
    m_context.vkCmdBindVertexBuffers(m_cmd_buf, 0, 1, &buffer, &offset);
}

void CommandBuffer::copy_buffer(vk::Buffer src, vk::Buffer dst, Span<vk::BufferCopy> regions) const {
    m_context.vkCmdCopyBuffer(m_cmd_buf, src, dst, regions.size(), regions.data());
}

void CommandBuffer::push_constants(vk::PipelineLayout layout, vk::ShaderStage stage, uint32_t size,
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

void CommandBuffer::pipeline_barrier(vk::PipelineStage src_stage, vk::PipelineStage dst_stage,
                                     Span<vk::BufferMemoryBarrier> buffer_barriers,
                                     Span<vk::ImageMemoryBarrier> image_barriers) const {
    m_context.vkCmdPipelineBarrier(m_cmd_buf, src_stage, dst_stage, vk::DependencyFlags::None, 0, nullptr,
                                   buffer_barriers.size(), buffer_barriers.data(), image_barriers.size(),
                                   image_barriers.data());
}

void CommandBuffer::reset_query_pool(vk::QueryPool query_pool, uint32_t query_count) const {
    m_context.vkCmdResetQueryPool(m_cmd_buf, query_pool, 0, query_count);
}

void CommandBuffer::write_timestamp(vk::PipelineStage stage, vk::QueryPool query_pool, uint32_t query) const {
    m_context.vkCmdWriteTimestamp(m_cmd_buf, stage, query_pool, query);
}

} // namespace vull
