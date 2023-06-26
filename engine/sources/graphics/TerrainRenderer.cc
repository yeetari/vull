#include <vull/graphics/TerrainRenderer.hh>

#include <vull/core/Log.hh>
#include <vull/graphics/GBuffer.hh>
#include <vull/maths/Vec.hh>
#include <vull/terrain/Chunk.hh>
#include <vull/terrain/Terrain.hh>
#include <vull/vulkan/CommandBuffer.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/DescriptorBuilder.hh>
#include <vull/vulkan/MemoryUsage.hh>
#include <vull/vulkan/PipelineBuilder.hh>
#include <vull/vulkan/Queue.hh>
#include <vull/vulkan/RenderGraph.hh>
#include <vull/vulkan/Sampler.hh>
#include <vull/vulkan/Shader.hh>

namespace vull {
namespace {

constexpr uint32_t k_height_map_resolution = 2048;
constexpr uint32_t k_tessellation_level = 2;

struct PushConstantBlock {
    Vec2f position;
    uint32_t terrain_size{0};
    uint32_t chunk_size{0};
    float morph_const_z{0.0f};
    float morph_const_w{0.0f};
};

} // namespace

TerrainRenderer::TerrainRenderer(vk::Context &context) : m_context(context) {
    Array set_bindings{
        // Frame UBO.
        vkb::DescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = vkb::DescriptorType::UniformBuffer,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Vertex,
        },
        // Height map.
        vkb::DescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = vkb::DescriptorType::CombinedImageSampler,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Vertex | vkb::ShaderStage::Fragment,
        },
    };
    vkb::DescriptorSetLayoutCreateInfo set_layout_ci{
        .sType = vkb::StructureType::DescriptorSetLayoutCreateInfo,
        .flags = vkb::DescriptorSetLayoutCreateFlags::DescriptorBufferEXT,
        .bindingCount = set_bindings.size(),
        .pBindings = set_bindings.data(),
    };
    VULL_ENSURE(m_context.vkCreateDescriptorSetLayout(&set_layout_ci, &m_set_layout) == vkb::Result::Success);
    m_context.vkGetDescriptorSetLayoutSizeEXT(m_set_layout, &m_set_layout_size);

    auto vertex_shader = VULL_EXPECT(vk::Shader::load(m_context, "/shaders/terrain.vert"));
    auto fragment_shader = VULL_EXPECT(vk::Shader::load(m_context, "/shaders/terrain.frag"));
    m_pipeline = VULL_EXPECT(vk::PipelineBuilder()
                                 .add_colour_attachment(vkb::Format::R8G8B8A8Unorm)
                                 .add_colour_attachment(vkb::Format::R16G16Snorm)
                                 .add_set_layout(m_set_layout)
                                 .add_shader(vertex_shader)
                                 .add_shader(fragment_shader)
                                 .set_cull_mode(vkb::CullMode::Back, vkb::FrontFace::CounterClockwise)
                                 .set_depth_format(vkb::Format::D32Sfloat)
                                 .set_depth_params(vkb::CompareOp::GreaterOrEqual, true, true)
                                 .set_polygon_mode(vkb::PolygonMode::Fill)
                                 .set_push_constant_range({
                                     .stageFlags = vkb::ShaderStage::Vertex | vkb::ShaderStage::Fragment,
                                     .size = sizeof(PushConstantBlock),
                                 })
                                 .set_topology(vkb::PrimitiveTopology::TriangleList)
                                 .build(m_context));
    m_wireframe_pipeline = VULL_EXPECT(vk::PipelineBuilder()
                                 .add_colour_attachment(vkb::Format::R8G8B8A8Unorm)
                                 .add_colour_attachment(vkb::Format::R16G16Snorm)
                                 .add_set_layout(m_set_layout)
                                 .add_shader(vertex_shader)
                                 .add_shader(fragment_shader)
                                 .set_cull_mode(vkb::CullMode::Back, vkb::FrontFace::CounterClockwise)
                                 .set_depth_format(vkb::Format::D32Sfloat)
                                 .set_depth_params(vkb::CompareOp::GreaterOrEqual, true, true)
                                 .set_polygon_mode(vkb::PolygonMode::Line)
                                 .set_push_constant_range({
                                     .stageFlags = vkb::ShaderStage::Vertex | vkb::ShaderStage::Fragment,
                                     .size = sizeof(PushConstantBlock),
                                 })
                                 .set_topology(vkb::PrimitiveTopology::TriangleList)
                                 .build(m_context));

    m_vertex_buffer = m_context.create_buffer((k_tessellation_level + 1) * (k_tessellation_level + 1) * sizeof(Vec2f),
                                              vkb::BufferUsage::VertexBuffer, vk::MemoryUsage::HostToDevice);
    auto *vertex_data = m_vertex_buffer.mapped<Vec2f>();
    for (uint32_t z = 0; z <= k_tessellation_level; z++) {
        for (uint32_t x = 0; x <= k_tessellation_level; x++) {
            *vertex_data++ =
                Vec2f(static_cast<float>(x), static_cast<float>(z)) / static_cast<float>(k_tessellation_level);
        }
    }

    m_index_buffer = m_context.create_buffer(k_tessellation_level * k_tessellation_level * 6 * sizeof(uint16_t),
                                             vkb::BufferUsage::IndexBuffer, vk::MemoryUsage::HostToDevice);
    auto *index_data = m_index_buffer.mapped<uint16_t>();
    for (uint16_t z = 0; z < k_tessellation_level; z++) {
        for (uint16_t x = 0; x < k_tessellation_level; x++) {
            const auto a = x + (k_tessellation_level + 1) * z;
            const auto b = x + (k_tessellation_level + 1) * (z + 1);
            const auto c = (x + 1) + (k_tessellation_level + 1) * (z + 1);
            const auto d = (x + 1) + (k_tessellation_level + 1) * z;
            *index_data++ = a;
            *index_data++ = b;
            *index_data++ = d;
            *index_data++ = b;
            *index_data++ = c;
            *index_data++ = d;
        }
    }

    vkb::ImageCreateInfo height_map_ci{
        .sType = vkb::StructureType::ImageCreateInfo,
        .imageType = vkb::ImageType::_2D,
        .format = vkb::Format::R32Sfloat,
        .extent = {k_height_map_resolution, k_height_map_resolution, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = vkb::SampleCount::_1,
        .tiling = vkb::ImageTiling::Optimal,
        .usage = vkb::ImageUsage::Sampled | vkb::ImageUsage::TransferDst,
        .sharingMode = vkb::SharingMode::Exclusive,
        .initialLayout = vkb::ImageLayout::Undefined,
    };
    m_height_map = m_context.create_image(height_map_ci, vk::MemoryUsage::DeviceOnly);
}

TerrainRenderer::~TerrainRenderer() {
    m_context.vkDestroyDescriptorSetLayout(m_set_layout);
}

uint32_t TerrainRenderer::build_pass(Terrain &terrain, vk::RenderGraph &graph, GBuffer &gbuffer,
                                     vk::ResourceId &frame_ubo) {
    Vector<Chunk *> chunks;
    terrain.update(m_view_position, chunks);
    const auto chunk_count = chunks.size();

    vk::BufferDescription descriptor_buffer_description{
        .size = m_set_layout_size,
        .usage = vkb::BufferUsage::ResourceDescriptorBufferEXT,
        .host_accessible = true,
    };
    auto descriptor_buffer_id = graph.new_buffer("terrain-descriptor-buffer", descriptor_buffer_description);

    auto &pass = graph.add_pass("terrain", vk::PassFlags::Graphics)
                     .read(frame_ubo)
                     .write(gbuffer.albedo, vk::WriteFlags::Additive)
                     .write(gbuffer.normal, vk::WriteFlags::Additive)
                     .write(gbuffer.depth, vk::WriteFlags::Additive);
    pass.set_on_execute([=, this, &terrain, &graph, chunks = vull::move(chunks)](vk::CommandBuffer &cmd_buf) {
        const auto &descriptor_buffer = graph.get_buffer(descriptor_buffer_id);
        vk::DescriptorBuilder descriptor_builder(m_set_layout, descriptor_buffer);
        descriptor_builder.set(0, 0, graph.get_buffer(frame_ubo));
        descriptor_builder.set(1, 0, m_height_map.full_view().sampled(vk::Sampler::Linear));

        cmd_buf.bind_descriptor_buffer(vkb::PipelineBindPoint::Graphics, descriptor_buffer, 0, 0);
        cmd_buf.bind_pipeline(m_wireframe ? m_wireframe_pipeline : m_pipeline);
        cmd_buf.bind_vertex_buffer(m_vertex_buffer);
        cmd_buf.bind_index_buffer(m_index_buffer, vkb::IndexType::Uint16);
        for (auto *chunk : chunks) {
            PushConstantBlock push_constant_block{
                .position = chunk->position(),
                .terrain_size = terrain.size(),
                .chunk_size = chunk->size(),
                .morph_const_z = chunk->morph_end() / (chunk->morph_end() - chunk->morph_start()),
                .morph_const_w = 1.0f / (chunk->morph_end() - chunk->morph_start()),
            };
            cmd_buf.push_constants(vkb::ShaderStage::Vertex | vkb::ShaderStage::Fragment, push_constant_block);
            cmd_buf.draw_indexed(static_cast<uint32_t>(m_index_buffer.size() / 2), 1);
        }
    });
    return chunk_count;
}

void TerrainRenderer::load_heights(uint32_t seed) {
    auto staging_buffer = m_context.create_buffer(k_height_map_resolution * k_height_map_resolution * sizeof(float),
                                                  vkb::BufferUsage::TransferSrc, vk::MemoryUsage::HostOnly);
    auto *height_data = staging_buffer.mapped<float>();
    for (uint32_t z = 0; z < k_height_map_resolution; z++) {
        for (uint32_t x = 0; x < k_height_map_resolution; x++) {
            height_data[x + z * k_height_map_resolution] =
                Terrain::height(seed, static_cast<float>(z), static_cast<float>(x));
        }
    }

    m_context.graphics_queue().immediate_submit([&](const vk::CommandBuffer &cmd_buf) {
        vkb::ImageMemoryBarrier2 transfer_write_barrier{
            .sType = vkb::StructureType::ImageMemoryBarrier2,
            .dstStageMask = vkb::PipelineStage2::Copy,
            .dstAccessMask = vkb::Access2::TransferWrite,
            .oldLayout = vkb::ImageLayout::Undefined,
            .newLayout = vkb::ImageLayout::TransferDstOptimal,
            .image = *m_height_map,
            .subresourceRange = m_height_map.full_view().range(),
        };
        vkb::BufferImageCopy copy{
            .imageSubresource{
                .aspectMask = vkb::ImageAspect::Color,
                .layerCount = 1,
            },
            .imageExtent = m_height_map.extent(),
        };
        vkb::ImageMemoryBarrier2 image_read_barrier{
            .sType = vkb::StructureType::ImageMemoryBarrier2,
            .srcStageMask = vkb::PipelineStage2::Copy,
            .srcAccessMask = vkb::Access2::TransferWrite,
            .dstStageMask = vkb::PipelineStage2::AllCommands,
            .dstAccessMask = vkb::Access2::ShaderRead,
            .oldLayout = vkb::ImageLayout::TransferDstOptimal,
            .newLayout = vkb::ImageLayout::ReadOnlyOptimal,
            .image = *m_height_map,
            .subresourceRange = m_height_map.full_view().range(),
        };
        cmd_buf.image_barrier(transfer_write_barrier);
        cmd_buf.copy_buffer_to_image(staging_buffer, m_height_map, vkb::ImageLayout::TransferDstOptimal, copy);
        cmd_buf.image_barrier(image_read_barrier);
    });
}

} // namespace vull
