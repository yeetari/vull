#pragma once

#include <vull/container/vector.hh>
#include <vull/support/span.hh>
#include <vull/support/string_view.hh>
#include <vull/vulkan/vulkan.hh>

#include <stdint.h>

namespace vull::vk {

class Buffer;
class Context;
class Image;
class Pipeline;
class QueryPool;
class Queue;

class CommandBuffer {
    friend Queue;

    struct DescriptorBufferBinding {
        vkb::PipelineBindPoint bind_point;
        uint32_t set;
        uint32_t buffer_index;
        vkb::DeviceSize offset;
    };

private:
    const Context &m_context;
    const vkb::CommandBuffer m_cmd_buf;
    vkb::Semaphore m_completion_semaphore;
    uint64_t m_completion_value{0};
    Vector<Buffer> m_associated_buffers;
    Vector<vkb::DescriptorBufferBindingInfoEXT> m_descriptor_buffers;
    Vector<DescriptorBufferBinding> m_descriptor_buffer_bindings;
    vkb::PipelineLayout m_compute_layout;
    vkb::PipelineLayout m_graphics_layout;
    bool m_in_flight{false};

    void reset();
    void emit_descriptor_binds();

public:
    CommandBuffer(const Context &context, vkb::CommandBuffer cmd_buf);
    CommandBuffer(const CommandBuffer &) = delete;
    CommandBuffer(CommandBuffer &&);
    ~CommandBuffer();

    CommandBuffer &operator=(const CommandBuffer &) = delete;
    CommandBuffer &operator=(CommandBuffer &&) = delete;

    void begin_label(StringView label);
    void insert_label(StringView label);
    void end_label();

    void set_viewport(Span<vkb::Viewport> viewports, uint32_t first = 0);
    void set_scissor(Span<vkb::Rect2D> scissors, uint32_t first = 0);

    void begin_rendering(const vkb::RenderingInfo &rendering_info) const;
    void end_rendering() const;

    void bind_associated_buffer(Buffer &&buffer);
    void bind_descriptor_buffer(vkb::PipelineBindPoint bind_point, const Buffer &buffer, uint32_t set,
                                vkb::DeviceSize offset);
    void bind_index_buffer(const Buffer &buffer, vkb::IndexType index_type) const;
    void bind_pipeline(const Pipeline &pipeline);
    void bind_vertex_buffer(const Buffer &buffer) const;

    void copy_buffer(const Buffer &src, const Buffer &dst, Span<vkb::BufferCopy> regions) const;
    void copy_buffer_to_image(const Buffer &src, const Image &dst, vkb::ImageLayout dst_layout,
                              Span<vkb::BufferImageCopy> regions) const;
    void zero_buffer(const Buffer &buffer, vkb::DeviceSize offset, vkb::DeviceSize size);

    void push_constants(vkb::ShaderStage stage, uint32_t size, const void *data) const;
    template <typename T>
    void push_constants(vkb::ShaderStage stage, const T &data) const;

    void dispatch(uint32_t x, uint32_t y = 1, uint32_t z = 1);
    void draw(uint32_t vertex_count, uint32_t instance_count);
    void draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index = 0);
    void draw_indexed_indirect_count(const Buffer &buffer, vkb::DeviceSize offset, const Buffer &count_buffer,
                                     vkb::DeviceSize count_offset, uint32_t max_draw_count, uint32_t stride);

    void buffer_barrier(const vkb::BufferMemoryBarrier2 &barrier) const;
    void image_barrier(const vkb::ImageMemoryBarrier2 &barrier) const;
    void pipeline_barrier(const vkb::DependencyInfo &dependency_info) const;
    void reset_query_pool(const QueryPool &query_pool) const;
    void reset_query(const QueryPool &query_pool, uint32_t query) const;
    void begin_query(const QueryPool &query_pool, uint32_t query) const;
    void end_query(const QueryPool &query_pool, uint32_t query) const;
    void write_timestamp(vkb::PipelineStage2 stage, const QueryPool &query_pool, uint32_t query) const;

    vkb::CommandBuffer operator*() const { return m_cmd_buf; }
    vkb::Semaphore completion_semaphore() const { return m_completion_semaphore; }
    uint64_t completion_value() const { return m_completion_value; }
};

template <typename T>
void CommandBuffer::push_constants(vkb::ShaderStage stage, const T &data) const {
    push_constants(stage, sizeof(T), &data);
}

} // namespace vull::vk
