#include <vull/vulkan/command_buffer.hh>

#include <vull/container/vector.hh>
#include <vull/support/assert.hh>
#include <vull/support/span.hh>
#include <vull/support/string_view.hh>
#include <vull/support/utility.hh>
#include <vull/vulkan/buffer.hh>
#include <vull/vulkan/context.hh>
#include <vull/vulkan/fence.hh>
#include <vull/vulkan/image.hh>
#include <vull/vulkan/pipeline.hh>
#include <vull/vulkan/query_pool.hh>
#include <vull/vulkan/vulkan.hh>

namespace vull::vk {

CommandBuffer::~CommandBuffer() {
    m_context.vkDestroyCommandPool(m_pool);
}

void CommandBuffer::reset() {
    // Reset the completion fence.
    m_completion_fence.reset();

    // Free any associated buffers.
    m_associated_buffers.clear();

    // Reset the command pool.
    m_context.vkResetCommandPool(m_pool, vkb::CommandPoolResetFlags::None);

    // Begin the buffer.
    vkb::CommandBufferBeginInfo cmd_buf_bi{
        .sType = vkb::StructureType::CommandBufferBeginInfo,
        .flags = vkb::CommandBufferUsage::OneTimeSubmit,
    };
    m_context.vkBeginCommandBuffer(m_buffer, &cmd_buf_bi);
}

void CommandBuffer::emit_descriptor_binds() {
    if (m_descriptor_buffers.empty()) {
        return;
    }
    m_context.vkCmdBindDescriptorBuffersEXT(m_buffer, m_descriptor_buffers.size(), m_descriptor_buffers.data());

    // TODO: Batch this.
    for (const auto &binding : m_descriptor_buffer_bindings) {
        auto *layout = binding.bind_point == vkb::PipelineBindPoint::Compute ? m_compute_layout : m_graphics_layout;
        m_context.vkCmdSetDescriptorBufferOffsetsEXT(m_buffer, binding.bind_point, layout, binding.set, 1,
                                                     &binding.buffer_index, &binding.offset);
    }
    m_descriptor_buffers.clear();
    m_descriptor_buffer_bindings.clear();
}

void CommandBuffer::begin_label(StringView label) {
    vkb::DebugUtilsLabelEXT label_info{
        .sType = vkb::StructureType::DebugUtilsLabelEXT,
        .pLabelName = label.data(),
        .color{1.0f, 1.0f, 1.0f, 1.0f},
    };
    m_context.vkCmdBeginDebugUtilsLabelEXT(m_buffer, &label_info);
}

void CommandBuffer::insert_label(StringView label) {
    vkb::DebugUtilsLabelEXT label_info{
        .sType = vkb::StructureType::DebugUtilsLabelEXT,
        .pLabelName = label.data(),
        .color{1.0f, 1.0f, 1.0f, 1.0f},
    };
    m_context.vkCmdInsertDebugUtilsLabelEXT(m_buffer, &label_info);
}

void CommandBuffer::end_label() {
    m_context.vkCmdEndDebugUtilsLabelEXT(m_buffer);
}

void CommandBuffer::set_viewport(Span<vkb::Viewport> viewports, uint32_t first) {
    m_context.vkCmdSetViewport(m_buffer, first, static_cast<uint32_t>(viewports.size()), viewports.data());
}

void CommandBuffer::set_scissor(Span<vkb::Rect2D> scissors, uint32_t first) {
    m_context.vkCmdSetScissor(m_buffer, first, static_cast<uint32_t>(scissors.size()), scissors.data());
}

void CommandBuffer::begin_rendering(const vkb::RenderingInfo &rendering_info) const {
    m_context.vkCmdBeginRendering(m_buffer, &rendering_info);
}

void CommandBuffer::end_rendering() const {
    m_context.vkCmdEndRendering(m_buffer);
}

void CommandBuffer::bind_associated_buffer(Buffer &&buffer) {
    m_associated_buffers.push(vull::move(buffer));
}

void CommandBuffer::bind_descriptor_buffer(vkb::PipelineBindPoint bind_point, const Buffer &buffer, uint32_t set,
                                           vkb::DeviceSize offset) {
    VULL_ASSERT((buffer.usage() & (vkb::BufferUsage::ResourceDescriptorBufferEXT |
                                   vkb::BufferUsage::SamplerDescriptorBufferEXT)) != static_cast<vkb::BufferUsage>(0));
    uint32_t buffer_index = m_descriptor_buffers.size();
    bool need_to_bind = true;
    for (uint32_t i = 0; i < m_descriptor_buffers.size(); i++) {
        if (m_descriptor_buffers[i].address == buffer.device_address()) {
            buffer_index = i;
            need_to_bind = false;
        }
    }
    m_descriptor_buffer_bindings.push({
        .bind_point = bind_point,
        .set = set,
        .buffer_index = buffer_index,
        .offset = offset,
    });
    if (need_to_bind) {
        m_descriptor_buffers.push({
            .sType = vkb::StructureType::DescriptorBufferBindingInfoEXT,
            .address = buffer.device_address(),
            .usage = buffer.usage(),
        });
    }
}

void CommandBuffer::bind_index_buffer(const Buffer &buffer, vkb::IndexType index_type) const {
    m_context.vkCmdBindIndexBuffer(m_buffer, *buffer, 0, index_type);
}

void CommandBuffer::bind_pipeline(const Pipeline &pipeline) {
    switch (pipeline.bind_point()) {
    case vkb::PipelineBindPoint::Compute:
        m_compute_layout = pipeline.layout();
        break;
    case vkb::PipelineBindPoint::Graphics:
        m_graphics_layout = pipeline.layout();
        break;
    default:
        vull::unreachable();
    }
    m_context.vkCmdBindPipeline(m_buffer, pipeline.bind_point(), *pipeline);
}

void CommandBuffer::bind_vertex_buffer(const Buffer &buffer) const {
    const vkb::DeviceSize offset = 0;
    auto *vk_buffer = *buffer;
    m_context.vkCmdBindVertexBuffers(m_buffer, 0, 1, &vk_buffer, &offset);
}

void CommandBuffer::copy_buffer(const Buffer &src, const Buffer &dst, Span<vkb::BufferCopy> regions) const {
    m_context.vkCmdCopyBuffer(m_buffer, *src, *dst, static_cast<uint32_t>(regions.size()), regions.data());
}

void CommandBuffer::copy_buffer_to_image(const Buffer &src, const Image &dst, vkb::ImageLayout dst_layout,
                                         Span<vkb::BufferImageCopy> regions) const {
    m_context.vkCmdCopyBufferToImage(m_buffer, *src, *dst, dst_layout, static_cast<uint32_t>(regions.size()),
                                     regions.data());
}

void CommandBuffer::zero_buffer(const Buffer &buffer, vkb::DeviceSize offset, vkb::DeviceSize size) {
    m_context.vkCmdFillBuffer(m_buffer, *buffer, offset, size, 0);
}

void CommandBuffer::push_constants(vkb::ShaderStage stage, uint32_t size, const void *data) const {
    VULL_ASSERT(stage != vkb::ShaderStage::All);
    if (stage == vkb::ShaderStage::Compute) {
        m_context.vkCmdPushConstants(m_buffer, m_compute_layout, stage, 0, size, data);
        return;
    }
    if (stage != vkb::ShaderStage::AllGraphics) {
        VULL_ASSERT((stage & vkb::ShaderStage::Compute) != vkb::ShaderStage::Compute);
    }
    m_context.vkCmdPushConstants(m_buffer, m_graphics_layout, stage, 0, size, data);
}

void CommandBuffer::dispatch(uint32_t x, uint32_t y, uint32_t z) {
    emit_descriptor_binds();
    m_context.vkCmdDispatch(m_buffer, x, y, z);
}

void CommandBuffer::draw(uint32_t vertex_count, uint32_t instance_count) {
    emit_descriptor_binds();
    m_context.vkCmdDraw(m_buffer, vertex_count, instance_count, 0, 0);
}

void CommandBuffer::draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index) {
    emit_descriptor_binds();
    m_context.vkCmdDrawIndexed(m_buffer, index_count, instance_count, first_index, 0, 0);
}

void CommandBuffer::draw_indexed_indirect_count(const Buffer &buffer, vkb::DeviceSize offset,
                                                const Buffer &count_buffer, vkb::DeviceSize count_offset,
                                                uint32_t max_draw_count, uint32_t stride) {
    emit_descriptor_binds();
    m_context.vkCmdDrawIndexedIndirectCount(m_buffer, *buffer, offset, *count_buffer, count_offset, max_draw_count,
                                            stride);
}

void CommandBuffer::buffer_barrier(const vkb::BufferMemoryBarrier2 &barrier) const {
    pipeline_barrier({
        .sType = vkb::StructureType::DependencyInfo,
        .bufferMemoryBarrierCount = 1,
        .pBufferMemoryBarriers = &barrier,
    });
}

void CommandBuffer::image_barrier(const vkb::ImageMemoryBarrier2 &barrier) const {
    pipeline_barrier({
        .sType = vkb::StructureType::DependencyInfo,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
    });
}

void CommandBuffer::pipeline_barrier(const vkb::DependencyInfo &dependency_info) const {
    m_context.vkCmdPipelineBarrier2(m_buffer, &dependency_info);
}

void CommandBuffer::reset_query_pool(const vk::QueryPool &query_pool) const {
    m_context.vkCmdResetQueryPool(m_buffer, *query_pool, 0, query_pool.count());
}

void CommandBuffer::reset_query(const QueryPool &query_pool, uint32_t query) const {
    m_context.vkCmdResetQueryPool(m_buffer, *query_pool, query, 1);
}

void CommandBuffer::begin_query(const QueryPool &query_pool, uint32_t query) const {
    m_context.vkCmdBeginQuery(m_buffer, *query_pool, query, vkb::QueryControlFlags::None);
}

void CommandBuffer::end_query(const QueryPool &query_pool, uint32_t query) const {
    m_context.vkCmdEndQuery(m_buffer, *query_pool, query);
}

void CommandBuffer::write_timestamp(vkb::PipelineStage2 stage, const QueryPool &query_pool, uint32_t query) const {
    // TODO: Neither amdvlk nor radv seem to handle None.
    if (stage == vkb::PipelineStage2::None) {
        stage = vkb::PipelineStage2::TopOfPipe;
    }
    m_context.vkCmdWriteTimestamp2(m_buffer, stage, *query_pool, query);
}

} // namespace vull::vk
