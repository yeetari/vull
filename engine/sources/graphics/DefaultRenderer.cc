#include <vull/graphics/DefaultRenderer.hh>

#include <vull/core/BoundingSphere.hh>
#include <vull/core/Scene.hh>
#include <vull/ecs/Entity.hh>
#include <vull/ecs/World.hh>
#include <vull/graphics/Material.hh>
#include <vull/graphics/Mesh.hh>
#include <vull/maths/Common.hh>
#include <vull/maths/Mat.hh>
#include <vull/maths/Projection.hh>
#include <vull/maths/Vec.hh>
#include <vull/support/Array.hh>
#include <vull/support/Assert.hh>
#include <vull/support/HashMap.hh>
#include <vull/support/HashSet.hh>
#include <vull/support/Optional.hh>
#include <vull/support/Result.hh>
#include <vull/support/String.hh>
#include <vull/support/Tuple.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>
#include <vull/vpak/PackFile.hh>
#include <vull/vpak/Reader.hh>
#include <vull/vulkan/Buffer.hh>
#include <vull/vulkan/CommandBuffer.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/DescriptorBuilder.hh>
#include <vull/vulkan/Image.hh>
#include <vull/vulkan/MemoryUsage.hh>
#include <vull/vulkan/Pipeline.hh>
#include <vull/vulkan/PipelineBuilder.hh>
#include <vull/vulkan/Queue.hh>
#include <vull/vulkan/RenderGraph.hh>
#include <vull/vulkan/Sampler.hh>
#include <vull/vulkan/Shader.hh>
#include <vull/vulkan/Vulkan.hh>

#include <float.h>
#include <stddef.h>
#include <string.h>

namespace vull {
namespace {

constexpr uint32_t k_cascade_count = 4;
constexpr uint32_t k_shadow_resolution = 2048;
constexpr uint32_t k_texture_limit = 32768;
constexpr uint32_t k_tile_size = 32;
constexpr uint32_t k_tile_max_light_count = 256;
constexpr uint32_t k_max_abuffer_depth = 8;

// Minimum required maximum work group count * minimum cull work group size that would be used.
constexpr uint32_t k_object_limit = 65535 * 32;

struct DepthReduceData {
    Vec2u mip_size;
};

struct DrawCmd : vkb::DrawIndexedIndirectCommand {
    uint32_t object_index;
};

struct Object {
    Mat4f transform;
    Vec3f center;
    float radius;
    uint32_t albedo_index;
    uint32_t normal_index;
    uint32_t index_count;
    uint32_t first_index;
    uint32_t vertex_offset;
};

struct ShadowPushConstantBlock {
    uint32_t cascade_index;
};

struct PointLight {
    Vec3f position;
    float radius{0.0f};
    Vec3f colour;
    float padding{0.0f};
};

struct Vertex {
    Vec3f position;
    Vec3f normal;
    Vec2f uv;
};

} // namespace

DefaultRenderer::DefaultRenderer(vk::Context &context, ShaderMap &&shader_map, vkb::Extent3D viewport_extent)
    : m_context(context), m_viewport_extent(viewport_extent), m_shader_map(vull::move(shader_map)) {
    m_tile_extent = {
        .width = vull::ceil_div(m_viewport_extent.width, k_tile_size),
        .height = vull::ceil_div(m_viewport_extent.height, k_tile_size),
    };
    create_set_layouts();
    create_resources();
    create_pipelines();
}

DefaultRenderer::~DefaultRenderer() {
    m_context.vkDestroyDescriptorSetLayout(m_reduce_set_layout);
    m_context.vkDestroyDescriptorSetLayout(m_texture_set_layout);
    m_context.vkDestroyDescriptorSetLayout(m_main_set_layout);
}

void DefaultRenderer::create_set_layouts() {
    Array main_set_bindings{
        // Frame UBO.
        vkb::DescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = vkb::DescriptorType::UniformBuffer,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::All,
        },
        // Light buffer.
        vkb::DescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = vkb::DescriptorType::StorageBuffer,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Compute,
        },
        // Object buffer.
        vkb::DescriptorSetLayoutBinding{
            .binding = 2,
            .descriptorType = vkb::DescriptorType::StorageBuffer,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Vertex | vkb::ShaderStage::Compute,
        },
        // Object visibility.
        vkb::DescriptorSetLayoutBinding{
            .binding = 3,
            .descriptorType = vkb::DescriptorType::StorageBuffer,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Compute,
        },
        // Draw buffer.
        vkb::DescriptorSetLayoutBinding{
            .binding = 4,
            .descriptorType = vkb::DescriptorType::StorageBuffer,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Vertex | vkb::ShaderStage::Compute,
        },
        // Albedo image.
        vkb::DescriptorSetLayoutBinding{
            .binding = 5,
            .descriptorType = vkb::DescriptorType::SampledImage,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Compute,
        },
        // Normal image.
        vkb::DescriptorSetLayoutBinding{
            .binding = 6,
            .descriptorType = vkb::DescriptorType::SampledImage,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Compute,
        },
        // Depth image.
        vkb::DescriptorSetLayoutBinding{
            .binding = 7,
            .descriptorType = vkb::DescriptorType::SampledImage,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Compute | vkb::ShaderStage::Fragment,
        },
        // Depth pyramid.
        vkb::DescriptorSetLayoutBinding{
            .binding = 8,
            .descriptorType = vkb::DescriptorType::CombinedImageSampler,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Compute,
        },
        // Light visibility.
        vkb::DescriptorSetLayoutBinding{
            .binding = 9,
            .descriptorType = vkb::DescriptorType::StorageBuffer,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Compute,
        },
        // Output image.
        vkb::DescriptorSetLayoutBinding{
            .binding = 10,
            .descriptorType = vkb::DescriptorType::StorageImage,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Compute,
        },
        // A-buffer list.
        vkb::DescriptorSetLayoutBinding{
            .binding = 11,
            .descriptorType = vkb::DescriptorType::StorageBuffer,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Compute | vkb::ShaderStage::Fragment,
        },
        // A-buffer fragment data.
        vkb::DescriptorSetLayoutBinding{
            .binding = 12,
            .descriptorType = vkb::DescriptorType::StorageBuffer,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Compute | vkb::ShaderStage::Fragment,
        },
    };
    vkb::DescriptorSetLayoutCreateInfo main_set_layout_ci{
        .sType = vkb::StructureType::DescriptorSetLayoutCreateInfo,
        .flags = vkb::DescriptorSetLayoutCreateFlags::DescriptorBufferEXT,
        .bindingCount = main_set_bindings.size(),
        .pBindings = main_set_bindings.data(),
    };
    VULL_ENSURE(m_context.vkCreateDescriptorSetLayout(&main_set_layout_ci, &m_main_set_layout) == vkb::Result::Success);

    vkb::DescriptorSetLayoutBinding texture_set_binding{
        .binding = 0,
        .descriptorType = vkb::DescriptorType::CombinedImageSampler,
        .descriptorCount = k_texture_limit,
        .stageFlags = vkb::ShaderStage::Fragment,
    };
    auto texture_set_binding_flags = vkb::DescriptorBindingFlags::VariableDescriptorCount;
    vkb::DescriptorSetLayoutBindingFlagsCreateInfo texture_set_binding_flags_ci{
        .sType = vkb::StructureType::DescriptorSetLayoutBindingFlagsCreateInfo,
        .bindingCount = 1,
        .pBindingFlags = &texture_set_binding_flags,
    };
    vkb::DescriptorSetLayoutCreateInfo texture_set_layout_ci{
        .sType = vkb::StructureType::DescriptorSetLayoutCreateInfo,
        .pNext = &texture_set_binding_flags_ci,
        .flags = vkb::DescriptorSetLayoutCreateFlags::DescriptorBufferEXT,
        .bindingCount = 1,
        .pBindings = &texture_set_binding,
    };
    VULL_ENSURE(m_context.vkCreateDescriptorSetLayout(&texture_set_layout_ci, &m_texture_set_layout) ==
                vkb::Result::Success);

    Array reduce_set_bindings{
        vkb::DescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = vkb::DescriptorType::CombinedImageSampler,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Compute,
        },
        vkb::DescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = vkb::DescriptorType::StorageImage,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Compute,
        },
    };
    vkb::DescriptorSetLayoutCreateInfo reduce_set_layout_ci{
        .sType = vkb::StructureType::DescriptorSetLayoutCreateInfo,
        .flags = vkb::DescriptorSetLayoutCreateFlags::DescriptorBufferEXT,
        .bindingCount = reduce_set_bindings.size(),
        .pBindings = reduce_set_bindings.data(),
    };
    VULL_ENSURE(m_context.vkCreateDescriptorSetLayout(&reduce_set_layout_ci, &m_reduce_set_layout) ==
                vkb::Result::Success);

    // TODO: Align up sizes to descriptorBufferOffsetAlignment.
    m_context.vkGetDescriptorSetLayoutSizeEXT(m_main_set_layout, &m_main_set_layout_size);
    m_context.vkGetDescriptorSetLayoutSizeEXT(m_texture_set_layout, &m_texture_set_layout_size);
    m_context.vkGetDescriptorSetLayoutSizeEXT(m_reduce_set_layout, &m_reduce_set_layout_size);
}

void DefaultRenderer::create_resources() {
    // Round down viewport to previous power of two.
    m_depth_pyramid_extent = {1u << vull::log2(m_viewport_extent.width), 1u << vull::log2(m_viewport_extent.height)};

    m_object_visibility_buffer = m_context.create_buffer(
        (k_object_limit * sizeof(uint32_t)) / 32, vkb::BufferUsage::StorageBuffer | vkb::BufferUsage::TransferDst,
        vk::MemoryUsage::DeviceOnly);
    m_context.graphics_queue().immediate_submit([this](vk::CommandBuffer &cmd_buf) {
        cmd_buf.zero_buffer(m_object_visibility_buffer, 0, m_object_visibility_buffer.size());
    });
}

void DefaultRenderer::create_pipelines() {
    struct SpecializationData {
        uint32_t viewport_width;
        uint32_t viewport_height;
        uint32_t tile_size;
        uint32_t tile_max_light_count;
        uint32_t row_tile_count;
        uint32_t max_abuffer_depth;
    } specialization_data{
        .viewport_width = m_viewport_extent.width,
        .viewport_height = m_viewport_extent.height,
        .tile_size = k_tile_size,
        .tile_max_light_count = k_tile_max_light_count,
        .row_tile_count = m_tile_extent.width,
        .max_abuffer_depth = k_max_abuffer_depth,
    };
    Array specialization_map_entries{
        vkb::SpecializationMapEntry{
            .constantID = 0,
            .offset = offsetof(SpecializationData, viewport_width),
            .size = sizeof(SpecializationData::viewport_width),
        },
        vkb::SpecializationMapEntry{
            .constantID = 1,
            .offset = offsetof(SpecializationData, viewport_height),
            .size = sizeof(SpecializationData::viewport_height),
        },
        vkb::SpecializationMapEntry{
            .constantID = 2,
            .offset = offsetof(SpecializationData, tile_size),
            .size = sizeof(SpecializationData::tile_size),
        },
        vkb::SpecializationMapEntry{
            .constantID = 3,
            .offset = offsetof(SpecializationData, tile_max_light_count),
            .size = sizeof(SpecializationData::tile_max_light_count),
        },
        vkb::SpecializationMapEntry{
            .constantID = 4,
            .offset = offsetof(SpecializationData, row_tile_count),
            .size = sizeof(SpecializationData::row_tile_count),
        },
        vkb::SpecializationMapEntry{
            .constantID = 5,
            .offset = offsetof(SpecializationData, max_abuffer_depth),
            .size = sizeof(SpecializationData::max_abuffer_depth),
        },
    };
    vkb::SpecializationInfo specialization_info{
        .mapEntryCount = specialization_map_entries.size(),
        .pMapEntries = specialization_map_entries.data(),
        .dataSize = sizeof(SpecializationData),
        .pData = &specialization_data,
    };

    vkb::SpecializationMapEntry late_map_entry{
        .constantID = 0,
        .size = sizeof(vkb::Bool),
    };
    vkb::Bool late = true;
    vkb::SpecializationInfo late_specialization_info{
        .mapEntryCount = 1,
        .pMapEntries = &late_map_entry,
        .dataSize = sizeof(vkb::Bool),
        .pData = &late,
    };

    m_gbuffer_pipeline = vk::PipelineBuilder()
                             .add_colour_attachment(vkb::Format::R8G8B8A8Unorm)
                             .add_colour_attachment(vkb::Format::R16G16Sfloat)
                             .add_set_layout(m_main_set_layout)
                             .add_set_layout(m_texture_set_layout)
                             .add_shader(*m_shader_map.get("gbuffer-vert"), specialization_info)
                             .add_shader(*m_shader_map.get("gbuffer-frag"), specialization_info)
                             .set_cull_mode(vkb::CullMode::Back, vkb::FrontFace::CounterClockwise)
                             .set_depth_format(vkb::Format::D32Sfloat)
                             .set_depth_params(vkb::CompareOp::GreaterOrEqual, true, true)
                             .set_topology(vkb::PrimitiveTopology::TriangleList)
                             .build(m_context);

    m_shadow_pipeline = vk::PipelineBuilder()
                            .add_set_layout(m_main_set_layout)
                            .add_shader(*m_shader_map.get("shadow"), specialization_info)
                            .set_cull_mode(vkb::CullMode::Back, vkb::FrontFace::CounterClockwise)
                            .set_depth_bias(2.0f, 5.0f)
                            .set_depth_format(vkb::Format::D32Sfloat)
                            .set_depth_params(vkb::CompareOp::LessOrEqual, true, true)
                            .set_push_constant_range({
                                .stageFlags = vkb::ShaderStage::Vertex,
                                .size = sizeof(ShadowPushConstantBlock),
                            })
                            .set_topology(vkb::PrimitiveTopology::TriangleList)
                            .build(m_context);

    m_depth_reduce_pipeline = vk::PipelineBuilder()
                                  .add_set_layout(m_reduce_set_layout)
                                  .add_shader(*m_shader_map.get("depth-reduce"))
                                  .set_push_constant_range({
                                      .stageFlags = vkb::ShaderStage::Compute,
                                      .size = sizeof(DepthReduceData),
                                  })
                                  .build(m_context);

    m_early_cull_pipeline = vk::PipelineBuilder()
                                .add_set_layout(m_main_set_layout)
                                .add_shader(*m_shader_map.get("draw-cull"))
                                .build(m_context);

    m_late_cull_pipeline = vk::PipelineBuilder()
                               .add_set_layout(m_main_set_layout)
                               .add_shader(*m_shader_map.get("draw-cull"), late_specialization_info)
                               .build(m_context);

    m_light_cull_pipeline = vk::PipelineBuilder()
                                .add_set_layout(m_main_set_layout)
                                .add_shader(*m_shader_map.get("light-cull"), specialization_info)
                                .build(m_context);

    m_deferred_pipeline = vk::PipelineBuilder()
                              .add_set_layout(m_main_set_layout)
                              .add_shader(*m_shader_map.get("deferred"), specialization_info)
                              .build(m_context);
}

void DefaultRenderer::load_scene(Scene &scene, vpak::Reader &pack_reader) {
    m_scene = &scene;
    m_texture_descriptor_buffer = m_context.create_buffer(
        scene.texture_count() * m_context.descriptor_size(vkb::DescriptorType::CombinedImageSampler),
        vkb::BufferUsage::SamplerDescriptorBufferEXT | vkb::BufferUsage::TransferDst, vk::MemoryUsage::DeviceOnly);

    auto texture_descriptor_staging_buffer = m_texture_descriptor_buffer.create_staging();
    vk::DescriptorBuilder descriptor_builder(m_texture_set_layout, texture_descriptor_staging_buffer);
    for (uint32_t i = 0; i < scene.texture_count(); i++) {
        descriptor_builder.set(0, i, scene.textures()[i]);
    }
    m_texture_descriptor_buffer.copy_from(texture_descriptor_staging_buffer, m_context.graphics_queue());

    vkb::DeviceSize vertex_buffer_size = 0;
    vkb::DeviceSize index_buffer_size = 0;
    HashSet<String> seen_vertex_buffers;
    for (auto [entity, mesh] : scene.world().view<Mesh>()) {
        if (seen_vertex_buffers.add(mesh.vertex_data_name())) {
            continue;
        }
        const auto vertices_size = pack_reader.stat(mesh.vertex_data_name())->size;
        const auto indices_size = pack_reader.stat(mesh.index_data_name())->size;
        m_mesh_infos.set(mesh.vertex_data_name(),
                         MeshInfo{
                             .index_count = static_cast<uint32_t>(indices_size / sizeof(uint32_t)),
                             .index_offset = static_cast<uint32_t>(index_buffer_size / sizeof(uint32_t)),
                             .vertex_offset = static_cast<int32_t>(vertex_buffer_size / sizeof(Vertex)),
                         });
        vertex_buffer_size += vertices_size;
        index_buffer_size += indices_size;
    }

    m_vertex_buffer =
        m_context.create_buffer(vertex_buffer_size, vkb::BufferUsage::VertexBuffer | vkb::BufferUsage::TransferDst,
                                vk::MemoryUsage::DeviceOnly);
    m_index_buffer = m_context.create_buffer(
        index_buffer_size, vkb::BufferUsage::IndexBuffer | vkb::BufferUsage::TransferDst, vk::MemoryUsage::DeviceOnly);

    seen_vertex_buffers.clear();
    vkb::DeviceSize vertex_buffer_offset = 0;
    vkb::DeviceSize index_buffer_offset = 0;
    for (auto [entity, mesh] : scene.world().view<Mesh>()) {
        if (seen_vertex_buffers.add(mesh.vertex_data_name())) {
            continue;
        }

        auto vertex_entry = *pack_reader.stat(mesh.vertex_data_name());
        auto vertex_stream = *pack_reader.open(mesh.vertex_data_name());
        auto staging_buffer =
            m_context.create_buffer(vertex_entry.size, vkb::BufferUsage::TransferSrc, vk::MemoryUsage::HostOnly);
        VULL_EXPECT(vertex_stream.read({staging_buffer.mapped_raw(), vertex_entry.size}));

        m_context.graphics_queue().immediate_submit([&](vk::CommandBuffer &cmd_buf) {
            vkb::BufferCopy copy{
                .dstOffset = vertex_buffer_offset,
                .size = vertex_entry.size,
            };
            cmd_buf.copy_buffer(staging_buffer, m_vertex_buffer, copy);
        });
        vertex_buffer_offset += vertex_entry.size;

        auto index_entry = *pack_reader.stat(mesh.index_data_name());
        auto index_stream = *pack_reader.open(mesh.index_data_name());
        staging_buffer =
            m_context.create_buffer(index_entry.size, vkb::BufferUsage::TransferSrc, vk::MemoryUsage::HostOnly);
        VULL_EXPECT(index_stream.read({staging_buffer.mapped_raw(), index_entry.size}));

        m_context.graphics_queue().immediate_submit([&](vk::CommandBuffer &cmd_buf) {
            vkb::BufferCopy copy{
                .dstOffset = index_buffer_offset,
                .size = index_entry.size,
            };
            cmd_buf.copy_buffer(staging_buffer, m_index_buffer, copy);
        });
        index_buffer_offset += index_entry.size;
    }
    VULL_ENSURE(vertex_buffer_offset == vertex_buffer_size);
    VULL_ENSURE(index_buffer_offset == index_buffer_size);
}

void DefaultRenderer::record_draws(vk::CommandBuffer &cmd_buf, const vk::Buffer &draw_buffer) {
    cmd_buf.bind_vertex_buffer(m_vertex_buffer);
    cmd_buf.bind_index_buffer(m_index_buffer, vkb::IndexType::Uint32);
    cmd_buf.draw_indexed_indirect_count(draw_buffer, sizeof(uint32_t), draw_buffer, 0, m_object_count, sizeof(DrawCmd));
}

Tuple<vk::ResourceId, vk::ResourceId, vk::ResourceId> DefaultRenderer::build_pass(vk::RenderGraph &graph,
                                                                                  vk::ResourceId target) {
    Vector<PointLight> lights;
    lights.push(PointLight{
        .position = Vec3f(0.0f),
        .radius = 1.0f,
        .colour = Vec3f(1.0f),
    });

    Vector<Object> objects;
    for (auto [entity, mesh, material] : m_scene->world().view<Mesh, Material>()) {
        const auto mesh_info = m_mesh_infos.get(mesh.vertex_data_name());
        if (!mesh_info) {
            continue;
        }

        auto albedo_index = m_scene->texture_index(material.albedo_name());
        auto normal_index = m_scene->texture_index(material.normal_name());
        if (!albedo_index || !normal_index) {
            continue;
        }

        auto bounding_sphere = entity.try_get<BoundingSphere>();
        objects.push({
            .transform = m_scene->get_transform_matrix(entity),
            .center = bounding_sphere ? bounding_sphere->center() : Vec3f(0.0f),
            .radius = bounding_sphere ? bounding_sphere->radius() : FLT_MAX,
            .albedo_index = *albedo_index,
            .normal_index = *normal_index,
            .index_count = mesh_info->index_count,
            .first_index = mesh_info->index_offset,
            .vertex_offset = static_cast<uint32_t>(mesh_info->vertex_offset),
        });
    }

    // Cap object count just in case.
    m_object_count = vull::min(objects.size(), k_object_limit);

    struct PassData {
        vk::ResourceId descriptor_buffer;
        vk::ResourceId frame_ubo;
        vk::ResourceId light_buffer;
        vk::ResourceId object_buffer;
        vk::ResourceId draw_buffer;
        vk::ResourceId abuffer_list;
        vk::ResourceId abuffer_data;
        vk::ResourceId albedo_image;
        vk::ResourceId normal_image;
        vk::ResourceId depth_image;
        vk::ResourceId depth_pyramid;
        vk::ResourceId light_visibility;
        Vector<PointLight> lights;
        Vector<Object> objects;
        vk::DescriptorBuilder descriptor_builder;
    };
    auto &data = graph.add_pass<PassData>(
        "setup-frame", vk::PassFlags::Transfer,
        [&](vk::PassBuilder &builder, PassData &data) {
            vk::BufferDescription descriptor_buffer_description{
                .size = m_main_set_layout_size,
                .usage = vkb::BufferUsage::SamplerDescriptorBufferEXT | vkb::BufferUsage::ResourceDescriptorBufferEXT,
                .host_accessible = true,
            };
            vk::BufferDescription frame_ubo_description{
                .size = sizeof(UniformBuffer),
                .usage = vkb::BufferUsage::UniformBuffer,
                .host_accessible = true,
            };
            vk::BufferDescription light_buffer_description{
                .size = lights.size_bytes() * 4 + sizeof(float),
                .usage = vkb::BufferUsage::StorageBuffer,
                .host_accessible = true,
            };
            vk::BufferDescription object_buffer_description{
                .size = objects.size_bytes(),
                .usage = vkb::BufferUsage::StorageBuffer,
                .host_accessible = true,
            };
            vk::BufferDescription abuffer_list_description{
                .size = 160 * 1024 * 1024,
                .usage = vkb::BufferUsage::StorageBuffer | vkb::BufferUsage::TransferDst,
            };
            vk::BufferDescription abuffer_data_description{
                .size = 640 * 1024 * 1024,
                .usage = vkb::BufferUsage::StorageBuffer,
            };
            data.descriptor_buffer = builder.new_buffer("descriptor-buffer", descriptor_buffer_description);
            data.frame_ubo = builder.new_buffer("frame-ubo", frame_ubo_description);
            data.light_buffer = builder.new_buffer("light-buffer", light_buffer_description);
            data.object_buffer = builder.new_buffer("object-buffer", object_buffer_description);
            data.abuffer_list = builder.new_buffer("abuffer-list", abuffer_list_description);
            data.abuffer_data = builder.new_buffer("abuffer-data", abuffer_data_description);
            data.lights = vull::move(lights);
            data.objects = vull::move(objects);
        },
        [this](vk::RenderGraph &graph, vk::CommandBuffer &cmd_buf, PassData &data) {
            const auto &frame_ubo = graph.get_buffer(data.frame_ubo);
            const auto &light_buffer = graph.get_buffer(data.light_buffer);
            const auto &object_buffer = graph.get_buffer(data.object_buffer);
            const auto &abuffer_list = graph.get_buffer(data.abuffer_list);
            const auto &abuffer_data = graph.get_buffer(data.abuffer_data);

            UniformBuffer frame_ubo_data{
                .proj = m_proj,
                .inv_proj = vull::inverse(m_proj),
                .view = m_view,
                .proj_view = m_proj * m_view,
                .inv_proj_view = vull::inverse(m_proj * m_view),
                .cull_view = m_cull_view,
                .view_position = m_view_position,
                .object_count = m_object_count,
                .frustum_planes = m_frustum_planes,
                .shadow_info = m_shadow_info,
            };
            memcpy(frame_ubo.mapped_raw(), &frame_ubo_data, sizeof(UniformBuffer));

            const auto lights = vull::move(data.lights);
            uint32_t light_count = lights.size();
            memcpy(light_buffer.mapped_raw(), &light_count, sizeof(uint32_t));
            memcpy(light_buffer.mapped<float>() + 4, lights.data(), lights.size_bytes());

            const auto objects = vull::move(data.objects);
            memcpy(object_buffer.mapped_raw(), objects.data(), objects.size_bytes());

            data.descriptor_builder = {m_main_set_layout, graph.get_buffer(data.descriptor_buffer)};
            data.descriptor_builder.set(0, 0, frame_ubo);
            data.descriptor_builder.set(1, 0, light_buffer);
            data.descriptor_builder.set(2, 0, object_buffer);
            data.descriptor_builder.set(3, 0, m_object_visibility_buffer);
            data.descriptor_builder.set(11, 0, abuffer_list);
            data.descriptor_builder.set(12, 0, abuffer_data);
            cmd_buf.zero_buffer(abuffer_list, 0, abuffer_list.size());
        });

    graph.add_pass(
        "early-cull", vk::PassFlags::Compute,
        [&](vk::PassBuilder &builder) {
            vk::BufferDescription draw_buffer_description{
                .size = sizeof(uint32_t) + m_object_count * sizeof(DrawCmd),
                .usage =
                    vkb::BufferUsage::StorageBuffer | vkb::BufferUsage::IndirectBuffer | vkb::BufferUsage::TransferDst,
            };
            data.frame_ubo = builder.read(data.frame_ubo);
            data.draw_buffer = builder.new_buffer("draw-buffer", draw_buffer_description);
        },
        [this, &data](vk::RenderGraph &graph, vk::CommandBuffer &cmd_buf) {
            const auto &descriptor_buffer = graph.get_buffer(data.descriptor_buffer);
            const auto &draw_buffer = graph.get_buffer(data.draw_buffer);
            cmd_buf.zero_buffer(draw_buffer, 0, sizeof(uint32_t));
            // TODO: This should be a separate pass so RG can add the barrier itself.
            cmd_buf.buffer_barrier({
                .sType = vkb::StructureType::BufferMemoryBarrier2,
                .srcStageMask = vkb::PipelineStage2::Copy,
                .srcAccessMask = vkb::Access2::TransferWrite,
                .dstStageMask = vkb::PipelineStage2::ComputeShader,
                .dstAccessMask = vkb::Access2::ShaderStorageRead,
                .buffer = *draw_buffer,
                .size = sizeof(uint32_t),
            });

            data.descriptor_builder.set(4, 0, draw_buffer);
            cmd_buf.bind_descriptor_buffer(vkb::PipelineBindPoint::Compute, descriptor_buffer, 0, 0);
            cmd_buf.bind_pipeline(m_early_cull_pipeline);
            cmd_buf.dispatch(vull::ceil_div(m_object_count, 32u));
        });

    graph.add_pass(
        "early-draw", vk::PassFlags::Graphics,
        [&](vk::PassBuilder &builder) {
            vk::AttachmentDescription albedo_description{
                .extent = {m_viewport_extent.width, m_viewport_extent.height},
                .format = vkb::Format::R8G8B8A8Unorm,
                .usage = vkb::ImageUsage::ColorAttachment | vkb::ImageUsage::Sampled,
            };
            vk::AttachmentDescription normal_description{
                .extent = {m_viewport_extent.width, m_viewport_extent.height},
                .format = vkb::Format::R16G16Sfloat,
                .usage = vkb::ImageUsage::ColorAttachment | vkb::ImageUsage::Sampled,
            };
            vk::AttachmentDescription depth_description{
                .extent = {m_viewport_extent.width, m_viewport_extent.height},
                .format = vkb::Format::D32Sfloat,
                .usage = vkb::ImageUsage::DepthStencilAttachment | vkb::ImageUsage::Sampled,
            };
            data.draw_buffer = builder.read(data.draw_buffer, vk::ReadFlags::Indirect);
            data.albedo_image = builder.new_attachment("albedo-image", albedo_description);
            data.normal_image = builder.new_attachment("normal-image", normal_description);
            data.depth_image = builder.new_attachment("depth-image", depth_description);
        },
        [this, &data](vk::RenderGraph &graph, vk::CommandBuffer &cmd_buf) {
            const auto &descriptor_buffer = graph.get_buffer(data.descriptor_buffer);
            const auto &draw_buffer = graph.get_buffer(data.draw_buffer);
            const auto &albedo_image = graph.get_image(data.albedo_image);
            const auto &normal_image = graph.get_image(data.normal_image);
            const auto &depth_image = graph.get_image(data.depth_image);
            data.descriptor_builder.set(5, 0, albedo_image.full_view().sampled(vk::Sampler::None));
            data.descriptor_builder.set(6, 0, normal_image.full_view().sampled(vk::Sampler::None));
            data.descriptor_builder.set(7, 0, depth_image.full_view().sampled(vk::Sampler::None));

            cmd_buf.bind_descriptor_buffer(vkb::PipelineBindPoint::Graphics, descriptor_buffer, 0, 0);
            cmd_buf.bind_descriptor_buffer(vkb::PipelineBindPoint::Graphics, m_texture_descriptor_buffer, 1, 0);
            cmd_buf.bind_pipeline(m_gbuffer_pipeline);
            record_draws(cmd_buf, draw_buffer);
        });

    graph.add_pass(
        "depth-reduce", vk::PassFlags::Compute,
        [&](vk::PassBuilder &builder) {
            // TODO: Make depth pyramid an imported resource so reduce pass can be conditionally disabled via
            //       m_cull_view_locked. This won't stall the pipeline since it's further into the frame, and will also
            //       reduce VRAM usage.
            const auto depth_pyramid_mip_count =
                vull::log2(vull::max(m_depth_pyramid_extent.width, m_depth_pyramid_extent.height)) + 1;
            vk::AttachmentDescription depth_pyramid_description{
                .extent = m_depth_pyramid_extent,
                .format = vkb::Format::R16Sfloat,
                .usage = vkb::ImageUsage::Storage | vkb::ImageUsage::Sampled,
                .mip_levels = depth_pyramid_mip_count,
            };
            data.depth_image = builder.read(data.depth_image);
            data.depth_pyramid = builder.new_attachment("depth-pyramid", depth_pyramid_description);
        },
        [this, &data](vk::RenderGraph &graph, vk::CommandBuffer &cmd_buf) {
            const auto &depth_image = graph.get_image(data.depth_image);
            const auto &depth_pyramid = graph.get_image(data.depth_pyramid);
            const uint32_t level_count = depth_pyramid.full_view().range().levelCount;
            data.descriptor_builder.set(8, 0, depth_pyramid.full_view().sampled(vk::Sampler::DepthReduce));

            auto descriptor_buffer =
                m_context.create_buffer(m_reduce_set_layout_size * level_count,
                                        vkb::BufferUsage::SamplerDescriptorBufferEXT, vk::MemoryUsage::HostToDevice);
            for (uint32_t i = 0; i < level_count; i++) {
                vk::DescriptorBuilder descriptor_builder(
                    m_context, m_reduce_set_layout, descriptor_buffer.mapped<uint8_t>() + i * m_reduce_set_layout_size);
                const auto &input_view = i != 0 ? depth_pyramid.level_view(i - 1) : depth_image.full_view();
                descriptor_builder.set(0, 0, input_view.sampled(vk::Sampler::DepthReduce));
                descriptor_builder.set(1, 0, depth_pyramid.level_view(i));
            }

            vkb::DeviceSize descriptor_offset = 0;
            cmd_buf.bind_pipeline(m_depth_reduce_pipeline);
            for (uint32_t i = 0; i < level_count; i++) {
                cmd_buf.bind_descriptor_buffer(vkb::PipelineBindPoint::Compute, descriptor_buffer, 0,
                                               descriptor_offset);
                descriptor_offset += m_reduce_set_layout_size;

                DepthReduceData shader_data{
                    .mip_size{
                        vull::max(m_depth_pyramid_extent.width >> i, 1u),
                        vull::max(m_depth_pyramid_extent.height >> i, 1u),
                    },
                };
                cmd_buf.push_constants(vkb::ShaderStage::Compute, shader_data);

                vkb::ImageMemoryBarrier2 sample_barrier{
                    .sType = vkb::StructureType::ImageMemoryBarrier2,
                    .srcStageMask = vkb::PipelineStage2::ComputeShader,
                    .srcAccessMask = vkb::Access2::ShaderStorageWrite,
                    .dstStageMask = vkb::PipelineStage2::ComputeShader,
                    .dstAccessMask = vkb::Access2::ShaderSampledRead,
                    .oldLayout = vkb::ImageLayout::General,
                    .newLayout = vkb::ImageLayout::ReadOnlyOptimal,
                    .image = *depth_pyramid,
                    .subresourceRange{
                        .aspectMask = vkb::ImageAspect::Color,
                        .baseMipLevel = i,
                        .levelCount = 1,
                        .layerCount = 1,
                    },
                };
                cmd_buf.dispatch(vull::ceil_div(shader_data.mip_size.x(), 32u),
                                 vull::ceil_div(shader_data.mip_size.y(), 32u));
                cmd_buf.image_barrier(sample_barrier);
            }
            cmd_buf.bind_associated_buffer(vull::move(descriptor_buffer));

            // TODO: Since we transitioned each mip manually to ReadOnlyOptimal, the render graph doesn't know, so we
            //       must transition it back to General.
            vkb::ImageMemoryBarrier2 general_barrier{
                .sType = vkb::StructureType::ImageMemoryBarrier2,
                .srcStageMask = vkb::PipelineStage2::ComputeShader,
                .srcAccessMask = vkb::Access2::ShaderStorageWrite,
                .dstStageMask = vkb::PipelineStage2::AllCommands,
                .dstAccessMask = vkb::Access2::ShaderSampledRead,
                .oldLayout = vkb::ImageLayout::ReadOnlyOptimal,
                .newLayout = vkb::ImageLayout::General,
                .image = *depth_pyramid,
                .subresourceRange = depth_pyramid.full_view().range(),
            };
            cmd_buf.image_barrier(general_barrier);
        });

    graph.add_pass(
        "late-cull", vk::PassFlags::Compute,
        [&](vk::PassBuilder &builder) {
            data.depth_pyramid = builder.read(data.depth_pyramid);
            data.draw_buffer = builder.write(data.draw_buffer);
        },
        [this, &data](vk::RenderGraph &graph, vk::CommandBuffer &cmd_buf) {
            const auto &descriptor_buffer = graph.get_buffer(data.descriptor_buffer);
            const auto &draw_buffer = graph.get_buffer(data.draw_buffer);
            // Prevent write-after-read.
            cmd_buf.buffer_barrier({
                .sType = vkb::StructureType::BufferMemoryBarrier2,
                .srcStageMask = vkb::PipelineStage2::DrawIndirect,
                .srcAccessMask = vkb::Access2::IndirectCommandRead,
                .dstStageMask = vkb::PipelineStage2::Copy,
                .dstAccessMask = vkb::Access2::TransferWrite,
                .buffer = *draw_buffer,
                .size = vkb::k_whole_size,
            });
            cmd_buf.zero_buffer(draw_buffer, 0, sizeof(uint32_t));
            cmd_buf.buffer_barrier({
                .sType = vkb::StructureType::BufferMemoryBarrier2,
                .srcStageMask = vkb::PipelineStage2::Copy,
                .srcAccessMask = vkb::Access2::TransferWrite,
                .dstStageMask = vkb::PipelineStage2::ComputeShader,
                .dstAccessMask = vkb::Access2::ShaderStorageRead,
                .buffer = *draw_buffer,
                .size = sizeof(uint32_t),
            });

            cmd_buf.bind_descriptor_buffer(vkb::PipelineBindPoint::Compute, descriptor_buffer, 0, 0);
            cmd_buf.bind_pipeline(m_late_cull_pipeline);
            cmd_buf.dispatch(vull::ceil_div(m_object_count, 32u));
        });

    graph.add_pass(
        "late-draw", vk::PassFlags::Graphics,
        [&](vk::PassBuilder &builder) {
            data.draw_buffer = builder.read(data.draw_buffer, vk::ReadFlags::Indirect);
            data.albedo_image = builder.write(data.albedo_image, vk::WriteFlags::Additive);
            data.normal_image = builder.write(data.normal_image, vk::WriteFlags::Additive);
            data.depth_image = builder.write(data.depth_image, vk::WriteFlags::Additive);
        },
        [this, &data](vk::RenderGraph &graph, vk::CommandBuffer &cmd_buf) {
            const auto &descriptor_buffer = graph.get_buffer(data.descriptor_buffer);
            const auto &draw_buffer = graph.get_buffer(data.draw_buffer);
            cmd_buf.bind_descriptor_buffer(vkb::PipelineBindPoint::Graphics, descriptor_buffer, 0, 0);
            cmd_buf.bind_descriptor_buffer(vkb::PipelineBindPoint::Graphics, m_texture_descriptor_buffer, 1, 0);
            cmd_buf.bind_pipeline(m_gbuffer_pipeline);
            record_draws(cmd_buf, draw_buffer);
        });

    graph.add_pass(
        "light-cull", vk::PassFlags::Compute,
        [&](vk::PassBuilder &builder) {
            vk::BufferDescription visibility_description{
                .size = (sizeof(uint32_t) + k_tile_max_light_count * sizeof(uint32_t)) * m_tile_extent.width *
                        m_tile_extent.height,
                .usage = vkb::BufferUsage::StorageBuffer,
            };
            data.depth_image = builder.read(data.depth_image);
            data.light_visibility = builder.new_buffer("light-visibility", visibility_description);
        },
        [this, &data](vk::RenderGraph &graph, vk::CommandBuffer &cmd_buf) {
            const auto &descriptor_buffer = graph.get_buffer(data.descriptor_buffer);
            data.descriptor_builder.set(9, 0, graph.get_buffer(data.light_visibility));

            cmd_buf.bind_descriptor_buffer(vkb::PipelineBindPoint::Compute, descriptor_buffer, 0, 0);
            cmd_buf.bind_pipeline(m_light_cull_pipeline);
            cmd_buf.dispatch(m_tile_extent.width, m_tile_extent.height);
        });

    const auto composite_image = graph.add_pass<vk::ResourceId>(
        "deferred", vk::PassFlags::Compute,
        [&](vk::PassBuilder &builder, vk::ResourceId &output) {
            vk::AttachmentDescription composite_image_description{
                .extent = {m_viewport_extent.width, m_viewport_extent.height},
                .format = vkb::Format::R16G16B16A16Sfloat,
                .usage = vkb::ImageUsage::Storage | vkb::ImageUsage::TransferSrc,
            };
            data.albedo_image = builder.read(data.albedo_image);
            data.normal_image = builder.read(data.normal_image);
            data.depth_image = builder.read(data.depth_image);
            data.depth_pyramid = builder.read(data.depth_pyramid);
            data.light_visibility = builder.read(data.light_visibility);
            output = builder.new_attachment("composite-image", composite_image_description);
        },
        [this, &data](vk::RenderGraph &graph, vk::CommandBuffer &cmd_buf, const vk::ResourceId &output) {
            data.descriptor_builder.set(10, 0, graph.get_image(output).full_view());
            const auto &descriptor_buffer = graph.get_buffer(data.descriptor_buffer);
            cmd_buf.bind_descriptor_buffer(vkb::PipelineBindPoint::Compute, descriptor_buffer, 0, 0);
            cmd_buf.bind_pipeline(m_deferred_pipeline);
            cmd_buf.dispatch(vull::ceil_div(m_viewport_extent.width, 8u), vull::ceil_div(m_viewport_extent.height, 8u));
        });

    target = graph.add_pass<vk::ResourceId>(
        "blit", vk::PassFlags::Transfer,
        [&](vk::PassBuilder &builder, vk::ResourceId &output) {
            builder.read(composite_image); // TODO: Properly.
            output = builder.write(target);
        },
        [this, composite_image](vk::RenderGraph &graph, vk::CommandBuffer &cmd_buf, const vk::ResourceId &output) {
            vkb::ImageBlit region{
                .srcSubresource{
                    .aspectMask = vkb::ImageAspect::Color,
                    .layerCount = 1,
                },
                .srcOffsets{
                    {},
                    {2560, 1440, 1},
                },
                .dstSubresource{
                    .aspectMask = vkb::ImageAspect::Color,
                    .layerCount = 1,
                },
                .dstOffsets{
                    {},
                    {2560, 1440, 1},
                },
            };
            m_context.vkCmdBlitImage(*cmd_buf, *graph.get_image(composite_image), vkb::ImageLayout::TransferSrcOptimal,
                                     *graph.get_image(output), vkb::ImageLayout::TransferDstOptimal, 1, &region,
                                     vkb::Filter::Nearest);
        });
    return vull::make_tuple(target, data.depth_image, data.descriptor_buffer);
}

void DefaultRenderer::update_cascades() {
    // TODO: Don't hardcode.
    const float near_plane = 0.1f;
    const float shadow_distance = 2000.0f;
    const float clip_range = shadow_distance - near_plane;
    const float split_lambda = 0.85f;
    Array<float, 4> split_distances;
    for (uint32_t i = 0; i < k_cascade_count; i++) {
        float p = static_cast<float>(i + 1) / static_cast<float>(k_cascade_count);
        float log = near_plane * vull::pow((near_plane + clip_range) / near_plane, p);
        float uniform = near_plane + clip_range * p;
        float d = split_lambda * (log - uniform) + uniform;
        split_distances[i] = (d - near_plane) / clip_range;
    }

    // Build cascade matrices.
    const auto aspect_ratio =
        static_cast<float>(m_viewport_extent.width) / static_cast<float>(m_viewport_extent.height);
    const auto inv_camera =
        vull::inverse(vull::perspective(aspect_ratio, vull::half_pi<float>, near_plane, shadow_distance) * m_view);
    float last_split_distance = 0.0f;
    for (uint32_t i = 0; i < k_cascade_count; i++) {
        Array<Vec3f, 8> frustum_corners{
            Vec3f(-1.0f, 1.0f, -1.0f), Vec3f(1.0f, 1.0f, -1.0f), Vec3f(1.0f, -1.0f, -1.0f), Vec3f(-1.0f, -1.0f, -1.0f),
            Vec3f(-1.0f, 1.0f, 1.0f),  Vec3f(1.0f, 1.0f, 1.0f),  Vec3f(1.0f, -1.0f, 1.0f),  Vec3f(-1.0f, -1.0f, 1.0f),
        };

        // Project corners into world space.
        for (auto &corner : frustum_corners) {
            Vec4f inv_corner = inv_camera * Vec4f(corner, 1.0f);
            corner = inv_corner / inv_corner.w();
        }

        for (uint32_t j = 0; j < 4; j++) {
            Vec3f dist = frustum_corners[j + 4] - frustum_corners[j];
            frustum_corners[j + 4] = frustum_corners[j] + (dist * split_distances[i]);
            frustum_corners[j] = frustum_corners[j] + (dist * last_split_distance);
        }

        Vec3f frustum_center;
        for (const auto &corner : frustum_corners) {
            frustum_center += corner;
        }
        frustum_center /= 8.0f;

        float radius = 0.0f;
        for (const auto &corner : frustum_corners) {
            float distance = vull::magnitude(corner - frustum_center);
            radius = vull::max(radius, distance);
        }
        radius = vull::ceil(radius * 16.0f) / 16.0f;

        // TODO: direction duplicated in shader.
        constexpr Vec3f direction(0.6f, 0.6f, -0.6f);
        constexpr Vec3f up(0.0f, 1.0f, 0.0f);
        auto proj = vull::ortho(-radius, radius, -radius, radius, 0.0f, radius * 2.0f);
        auto view = vull::look_at(frustum_center + direction * radius, frustum_center, up);

        // Apply a small correction factor to the projection matrix to snap texels and avoid shimmering around the
        // edges of shadows.
        Vec4f origin = (proj * view * Vec4f(0.0f, 0.0f, 0.0f, 1.0f)) * (k_shadow_resolution / 2.0f);
        Vec2f rounded_origin(vull::round(origin.x()), vull::round(origin.y()));
        Vec2f round_offset = (rounded_origin - origin) * (2.0f / k_shadow_resolution);
        proj[3] += Vec4f(round_offset, 0.0f, 0.0f);

        m_shadow_info.cascade_matrices[i] = proj * view;
        m_shadow_info.cascade_split_depths[i] = (near_plane + split_distances[i] * clip_range);
        last_split_distance = split_distances[i];
    }
}

void DefaultRenderer::update_globals(const Mat4f &proj, const Mat4f &view, const Vec3f &view_position) {
    m_proj = proj;
    m_view = view;
    m_view_position = view_position;
    if (!m_cull_view_locked) {
        auto proj_view_t = vull::transpose(m_proj * m_view);
        m_frustum_planes[0] = proj_view_t[3] + proj_view_t[0]; // left
        m_frustum_planes[1] = proj_view_t[3] - proj_view_t[0]; // right
        m_frustum_planes[2] = proj_view_t[3] + proj_view_t[1]; // bottom
        m_frustum_planes[3] = proj_view_t[3] - proj_view_t[1]; // top
        for (auto &plane : m_frustum_planes) {
            plane /= vull::magnitude(Vec3f(plane));
        }
        m_cull_view = m_view;
    }
    update_cascades();
}

} // namespace vull
