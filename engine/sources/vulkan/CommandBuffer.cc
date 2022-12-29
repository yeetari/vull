#include <vull/vulkan/CommandBuffer.hh>

#include <vull/support/Assert.hh>
#include <vull/support/Span.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>
#include <vull/vulkan/Buffer.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/Image.hh>
#include <vull/vulkan/Pipeline.hh>
#include <vull/vulkan/QueryPool.hh>
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
    m_associated_buffers = vull::move(other.m_associated_buffers);
}

CommandBuffer::~CommandBuffer() {
    if (m_completion_semaphore == nullptr) {
        return;
    }
    [[maybe_unused]] uint64_t value;
    VULL_ASSERT(m_context.vkGetSemaphoreCounterValue(m_completion_semaphore, &value) == vkb::Result::Success);
    VULL_ASSERT(value == m_completion_value);
    m_context.vkDestroySemaphore(m_completion_semaphore);
}

void CommandBuffer::reset() {
    // Free any associated buffers.
    m_associated_buffers.clear();

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

void CommandBuffer::emit_descriptor_binds() {
    if (m_descriptor_buffers.empty()) {
        return;
    }
    m_context.vkCmdBindDescriptorBuffersEXT(m_cmd_buf, m_descriptor_buffers.size(), m_descriptor_buffers.data());

    // TODO: Batch this.
    for (const auto &binding : m_descriptor_buffer_bindings) {
        auto *layout = binding.bind_point == vkb::PipelineBindPoint::Compute ? m_compute_layout : m_graphics_layout;
        m_context.vkCmdSetDescriptorBufferOffsetsEXT(m_cmd_buf, binding.bind_point, layout, binding.set, 1,
                                                     &binding.buffer_index, &binding.offset);
    }
    m_descriptor_buffers.clear();
    m_descriptor_buffer_bindings.clear();
}

void CommandBuffer::begin_rendering(const vkb::RenderingInfo &rendering_info) const {
    m_context.vkCmdBeginRendering(m_cmd_buf, &rendering_info);
}

void CommandBuffer::end_rendering() const {
    m_context.vkCmdEndRendering(m_cmd_buf);
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
    m_context.vkCmdBindIndexBuffer(m_cmd_buf, *buffer, 0, index_type);
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
    m_context.vkCmdBindPipeline(m_cmd_buf, pipeline.bind_point(), *pipeline);
}

void CommandBuffer::bind_vertex_buffer(const Buffer &buffer) const {
    const vkb::DeviceSize offset = 0;
    auto *vk_buffer = *buffer;
    m_context.vkCmdBindVertexBuffers(m_cmd_buf, 0, 1, &vk_buffer, &offset);
}

void CommandBuffer::copy_buffer(const Buffer &src, const Buffer &dst, Span<vkb::BufferCopy> regions) const {
    m_context.vkCmdCopyBuffer(m_cmd_buf, *src, *dst, regions.size(), regions.data());
}

void CommandBuffer::copy_buffer_to_image(const Buffer &src, const Image &dst, vkb::ImageLayout dst_layout,
                                         Span<vkb::BufferImageCopy> regions) const {
    m_context.vkCmdCopyBufferToImage(m_cmd_buf, *src, *dst, dst_layout, regions.size(), regions.data());
}

void CommandBuffer::push_constants(vkb::ShaderStage stage, uint32_t size, const void *data) const {
    VULL_ASSERT(stage != vkb::ShaderStage::All);
    if (stage == vkb::ShaderStage::Compute) {
        m_context.vkCmdPushConstants(m_cmd_buf, m_compute_layout, stage, 0, size, data);
        return;
    }
    if (stage != vkb::ShaderStage::AllGraphics) {
        VULL_ASSERT((stage & vkb::ShaderStage::Compute) != vkb::ShaderStage::Compute);
    }
    m_context.vkCmdPushConstants(m_cmd_buf, m_graphics_layout, stage, 0, size, data);
}

void CommandBuffer::dispatch(uint32_t x, uint32_t y, uint32_t z) {
    emit_descriptor_binds();
    m_context.vkCmdDispatch(m_cmd_buf, x, y, z);
}

void CommandBuffer::draw(uint32_t vertex_count, uint32_t instance_count) {
    emit_descriptor_binds();
    m_context.vkCmdDraw(m_cmd_buf, vertex_count, instance_count, 0, 0);
}

void CommandBuffer::draw_indexed(uint32_t index_count, uint32_t instance_count) {
    emit_descriptor_binds();
    m_context.vkCmdDrawIndexed(m_cmd_buf, index_count, instance_count, 0, 0, 0);
}

void CommandBuffer::draw_indexed_indirect_count(const Buffer &buffer, vkb::DeviceSize offset,
                                                const Buffer &count_buffer, vkb::DeviceSize count_offset,
                                                uint32_t max_draw_count, uint32_t stride) {
    emit_descriptor_binds();
    m_context.vkCmdDrawIndexedIndirectCount(m_cmd_buf, *buffer, offset, *count_buffer, count_offset, max_draw_count,
                                            stride);
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

void CommandBuffer::reset_query_pool(const vk::QueryPool &query_pool) const {
    m_context.vkCmdResetQueryPool(m_cmd_buf, *query_pool, 0, query_pool.count());
}

void CommandBuffer::write_timestamp(vkb::PipelineStage2 stage, const QueryPool &query_pool, uint32_t query) const {
    // TODO: Neither amdvlk nor radv seem to handle None.
    if (stage == vkb::PipelineStage2::None) {
        stage = vkb::PipelineStage2::TopOfPipe;
    }
    m_context.vkCmdWriteTimestamp2(m_cmd_buf, stage, *query_pool, query);
}

} // namespace vull::vk
