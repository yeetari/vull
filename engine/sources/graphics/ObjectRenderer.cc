#include <vull/graphics/ObjectRenderer.hh>

#include <vull/graphics/Mesh.hh>
#include <vull/maths/Common.hh>
#include <vull/maths/Mat.hh>
#include <vull/maths/Projection.hh>
#include <vull/maths/Vec.hh>
#include <vull/support/Optional.hh>
#include <vull/support/Result.hh>
#include <vull/support/String.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/vpak/FileSystem.hh>
#include <vull/vpak/PackFile.hh>
#include <vull/vpak/Reader.hh>
#include <vull/vulkan/Buffer.hh>
#include <vull/vulkan/CommandBuffer.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/Image.hh>
#include <vull/vulkan/MemoryUsage.hh>
#include <vull/vulkan/Pipeline.hh>
#include <vull/vulkan/PipelineBuilder.hh>
#include <vull/vulkan/RenderGraph.hh>
#include <vull/vulkan/Shader.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stdint.h>

namespace vull {

ObjectRenderer::ObjectRenderer(vk::Context &context) : m_context(context) {
    auto vertex_shader = VULL_EXPECT(vk::Shader::load(m_context, "/shaders/object.vert"));
    auto fragment_shader = VULL_EXPECT(vk::Shader::load(m_context, "/shaders/object.frag"));
    m_pipeline =
        VULL_EXPECT(vk::PipelineBuilder()
                        .add_colour_attachment(vkb::Format::R8G8B8A8Unorm)
                        .add_shader(vertex_shader)
                        .add_shader(fragment_shader)
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
