#include <vull/graphics/object_renderer.hh>

#include <vull/graphics/mesh.hh>
#include <vull/maths/common.hh>
#include <vull/maths/mat.hh>
#include <vull/maths/projection.hh>
#include <vull/maths/vec.hh>
#include <vull/support/function.hh>
#include <vull/support/optional.hh>
#include <vull/support/result.hh>
#include <vull/support/span.hh>
#include <vull/support/string.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/vpak/defs.hh>
#include <vull/vpak/file_system.hh>
#include <vull/vpak/stream.hh>
#include <vull/vulkan/buffer.hh>
#include <vull/vulkan/command_buffer.hh>
#include <vull/vulkan/context.hh>
#include <vull/vulkan/image.hh>
#include <vull/vulkan/memory_usage.hh>
#include <vull/vulkan/pipeline.hh>
#include <vull/vulkan/pipeline_builder.hh>
#include <vull/vulkan/render_graph.hh>
#include <vull/vulkan/shader.hh>
#include <vull/vulkan/vulkan.hh>

#include <stdint.h>

namespace vull {

ObjectRenderer::ObjectRenderer(vk::Context &context) : m_context(context) {
    auto shader = VULL_EXPECT(vk::Shader::load(m_context, "/shaders/object"));
    m_pipeline =
        VULL_EXPECT(vk::PipelineBuilder()
                        .add_colour_attachment(vkb::Format::R8G8B8A8Unorm)
                        .add_shader(shader)
                        .set_topology(vkb::PrimitiveTopology::TriangleList)
                        .set_push_constant_range({.stageFlags = vkb::ShaderStage::Vertex, .size = sizeof(Mat4f)})
                        .set_depth_format(vkb::Format::D16Unorm)
                        .set_depth_params(vkb::CompareOp::GreaterOrEqual, true, true)
                        .build(m_context));
}

void ObjectRenderer::build_pass(vk::RenderGraph &graph, vk::ResourceId &target) {
    auto image_extent = graph.get_image(target).extent();
    vk::AttachmentDescription depth_description{
        .extent = {image_extent.width, image_extent.height},
        .format = vkb::Format::D16Unorm,
        .usage = vkb::ImageUsage::DepthStencilAttachment,
    };
    auto depth_image_id = graph.new_attachment("single-object-depth-image", depth_description);
    auto &pass = graph.add_pass("single-object", vk::PassFlags::Graphics).write(target).write(depth_image_id);
    pass.set_on_execute([=, this](vk::CommandBuffer &cmd_buf) {
        cmd_buf.bind_pipeline(m_pipeline);
        cmd_buf.push_constants(vkb::ShaderStage::Vertex,
                               vull::infinite_perspective(1.0f, 0.75f * vull::half_pi<float>, 0.1f) *
                                   vull::look_at(Vec3f{0.0f, 0.0f, -3.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}) *
                                   vull::rotation_y(m_rotation));
        cmd_buf.bind_vertex_buffer(m_vertex_buffer);
        cmd_buf.bind_index_buffer(m_index_buffer, vkb::IndexType::Uint32);
        cmd_buf.draw_indexed(static_cast<uint32_t>(m_index_buffer.size() / 4), 1);
    });
}

void ObjectRenderer::load(const Mesh &mesh) {
    const auto vertices_size = vpak::stat(mesh.vertex_data_name())->size;
    const auto indices_size = vpak::stat(mesh.index_data_name())->size;

    m_vertex_buffer =
        m_context.create_buffer(vertices_size, vkb::BufferUsage::VertexBuffer, vk::MemoryUsage::HostToDevice);
    m_index_buffer =
        m_context.create_buffer(indices_size, vkb::BufferUsage::IndexBuffer, vk::MemoryUsage::HostToDevice);

    auto vertex_stream = vpak::open(mesh.vertex_data_name());
    VULL_EXPECT(vertex_stream->read({m_vertex_buffer.mapped_raw(), vertices_size}));

    auto index_stream = vpak::open(mesh.index_data_name());
    VULL_EXPECT(index_stream->read({m_index_buffer.mapped_raw(), indices_size}));
}

} // namespace vull
