#include <vull/graphics/default_renderer.hh>

#include <vull/container/array.hh>
#include <vull/container/hash_map.hh>
#include <vull/container/hash_set.hh>
#include <vull/container/vector.hh>
#include <vull/core/bounding_sphere.hh>
#include <vull/ecs/entity.hh>
#include <vull/ecs/world.hh>
#include <vull/graphics/gbuffer.hh>
#include <vull/graphics/material.hh>
#include <vull/graphics/mesh.hh>
#include <vull/graphics/texture_streamer.hh>
#include <vull/graphics/vertex.hh>
#include <vull/maths/common.hh>
#include <vull/maths/mat.hh>
#include <vull/maths/vec.hh>
#include <vull/scene/camera.hh>
#include <vull/scene/scene.hh>
#include <vull/support/assert.hh>
#include <vull/support/function.hh>
#include <vull/support/optional.hh>
#include <vull/support/result.hh>
#include <vull/support/span.hh>
#include <vull/support/string.hh>
#include <vull/support/string_view.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>
#include <vull/tasklet/future.hh>
#include <vull/vpak/defs.hh>
#include <vull/vpak/file_system.hh>
#include <vull/vpak/stream.hh>
#include <vull/vulkan/buffer.hh>
#include <vull/vulkan/command_buffer.hh>
#include <vull/vulkan/context.hh>
#include <vull/vulkan/descriptor_builder.hh>
#include <vull/vulkan/image.hh>
#include <vull/vulkan/memory_usage.hh>
#include <vull/vulkan/pipeline.hh>
#include <vull/vulkan/pipeline_builder.hh>
#include <vull/vulkan/queue.hh>
#include <vull/vulkan/render_graph.hh>
#include <vull/vulkan/sampler.hh>
#include <vull/vulkan/shader.hh>
#include <vull/vulkan/vulkan.hh>

#include <float.h>
#include <string.h>

namespace vull {
namespace {

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

struct UniformBuffer {
    Mat4f proj;
    Mat4f inv_proj;
    Mat4f view;
    Mat4f proj_view;
    Mat4f inv_proj_view;
    Mat4f cull_view;
    Vec3f view_position;
    uint32_t object_count;
    Array<Vec4f, 4> frustum_planes;
};

} // namespace

DefaultRenderer::DefaultRenderer(vk::Context &context, vkb::Extent3D viewport_extent)
    : m_context(context), m_viewport_extent(viewport_extent), m_texture_streamer(context) {
    create_set_layouts();
    create_resources();
    create_pipelines();
}

DefaultRenderer::~DefaultRenderer() {
    m_context.vkDestroyDescriptorSetLayout(m_reduce_set_layout);
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
        // Object buffer.
        vkb::DescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = vkb::DescriptorType::StorageBuffer,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Vertex | vkb::ShaderStage::Compute,
        },
        // Object visibility buffer.
        vkb::DescriptorSetLayoutBinding{
            .binding = 2,
            .descriptorType = vkb::DescriptorType::StorageBuffer,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Compute,
        },
        // Draw buffer.
        vkb::DescriptorSetLayoutBinding{
            .binding = 3,
            .descriptorType = vkb::DescriptorType::StorageBuffer,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Vertex | vkb::ShaderStage::Compute,
        },
        // Depth pyramid.
        vkb::DescriptorSetLayoutBinding{
            .binding = 4,
            .descriptorType = vkb::DescriptorType::CombinedImageSampler,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Compute,
        },
        // Vertex buffer.
        vkb::DescriptorSetLayoutBinding{
            .binding = 5,
            .descriptorType = vkb::DescriptorType::StorageBuffer,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Vertex,
        },
    };
    vkb::DescriptorSetLayoutCreateInfo main_set_layout_ci{
        .sType = vkb::StructureType::DescriptorSetLayoutCreateInfo,
        .flags = vkb::DescriptorSetLayoutCreateFlags::DescriptorBufferEXT,
        .bindingCount = main_set_bindings.size(),
        .pBindings = main_set_bindings.data(),
    };
    VULL_ENSURE(m_context.vkCreateDescriptorSetLayout(&main_set_layout_ci, &m_main_set_layout) == vkb::Result::Success);

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
    m_context.vkGetDescriptorSetLayoutSizeEXT(m_reduce_set_layout, &m_reduce_set_layout_size);
}

void DefaultRenderer::create_resources() {
    // Round down viewport to previous power of two.
    m_depth_pyramid_extent = {1u << vull::log2(m_viewport_extent.width), 1u << vull::log2(m_viewport_extent.height)};

    m_object_visibility_buffer = m_context.create_buffer(
        (k_object_limit * sizeof(uint32_t)) / 32, vkb::BufferUsage::StorageBuffer | vkb::BufferUsage::TransferDst,
        vk::MemoryUsage::DeviceOnly);

    auto &queue = m_context.get_queue(vk::QueueKind::Transfer);
    auto cmd_buf = queue.request_cmd_buf();
    cmd_buf->zero_buffer(m_object_visibility_buffer, 0, m_object_visibility_buffer.size());
    queue.submit(vull::move(cmd_buf), {}, {}).await();
}

void DefaultRenderer::create_pipelines() {
    auto gbuffer_vert = VULL_EXPECT(vk::Shader::load(m_context, "/shaders/default.vert"));
    auto gbuffer_frag = VULL_EXPECT(vk::Shader::load(m_context, "/shaders/default.frag"));
    m_gbuffer_pipeline = VULL_EXPECT(vk::PipelineBuilder()
                                         .add_colour_attachment(vkb::Format::R8G8B8A8Unorm)
                                         .add_colour_attachment(vkb::Format::R16G16Snorm)
                                         .add_set_layout(m_main_set_layout)
                                         .add_set_layout(m_texture_streamer.set_layout())
                                         .add_shader(gbuffer_vert)
                                         .add_shader(gbuffer_frag)
                                         .set_cull_mode(vkb::CullMode::Back, vkb::FrontFace::CounterClockwise)
                                         .set_depth_format(vkb::Format::D32Sfloat)
                                         .set_depth_params(vkb::CompareOp::GreaterOrEqual, true, true)
                                         .set_topology(vkb::PrimitiveTopology::TriangleList)
                                         .build(m_context));

    auto shadow_shader = VULL_EXPECT(vk::Shader::load(m_context, "/shaders/shadow.vert"));
    m_shadow_pipeline = VULL_EXPECT(vk::PipelineBuilder()
                                        .add_set_layout(m_main_set_layout)
                                        .add_shader(shadow_shader)
                                        .set_cull_mode(vkb::CullMode::Back, vkb::FrontFace::CounterClockwise)
                                        .set_depth_bias(2.0f, 5.0f)
                                        .set_depth_format(vkb::Format::D32Sfloat)
                                        .set_depth_params(vkb::CompareOp::LessOrEqual, true, true)
                                        .set_push_constant_range({
                                            .stageFlags = vkb::ShaderStage::Vertex,
                                            .size = sizeof(ShadowPushConstantBlock),
                                        })
                                        .set_topology(vkb::PrimitiveTopology::TriangleList)
                                        .build(m_context));

    auto depth_reduce_shader = VULL_EXPECT(vk::Shader::load(m_context, "/shaders/depth_reduce.comp"));
    m_depth_reduce_pipeline = VULL_EXPECT(vk::PipelineBuilder()
                                              .add_set_layout(m_reduce_set_layout)
                                              .add_shader(depth_reduce_shader)
                                              .set_push_constant_range({
                                                  .stageFlags = vkb::ShaderStage::Compute,
                                                  .size = sizeof(DepthReduceData),
                                              })
                                              .build(m_context));

    auto draw_cull_shader = VULL_EXPECT(vk::Shader::load(m_context, "/shaders/draw_cull.comp"));
    m_early_cull_pipeline = VULL_EXPECT(vk::PipelineBuilder()
                                            .add_set_layout(m_main_set_layout)
                                            .add_shader(draw_cull_shader)
                                            .set_constant("k_late", false)
                                            .build(m_context));

    m_late_cull_pipeline = VULL_EXPECT(vk::PipelineBuilder()
                                           .add_set_layout(m_main_set_layout)
                                           .add_shader(draw_cull_shader)
                                           .set_constant("k_late", true)
                                           .build(m_context));
}

void DefaultRenderer::load_scene(Scene &scene) {
    m_scene = &scene;

    vkb::DeviceSize vertex_buffer_size = 0;
    vkb::DeviceSize index_buffer_size = 0;
    HashSet<String> seen_vertex_buffers;
    for (auto [entity, mesh] : scene.world().view<Mesh>()) {
        if (seen_vertex_buffers.add(mesh.vertex_data_name())) {
            continue;
        }
        const auto vertices_size = vpak::stat(mesh.vertex_data_name())->size;
        const auto indices_size = vpak::stat(mesh.index_data_name())->size;
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
        m_context.create_buffer(vertex_buffer_size, vkb::BufferUsage::StorageBuffer | vkb::BufferUsage::TransferDst,
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

        auto vertex_entry = *vpak::stat(mesh.vertex_data_name());
        auto vertex_stream = vpak::open(mesh.vertex_data_name());
        auto staging_buffer =
            m_context.create_buffer(vertex_entry.size, vkb::BufferUsage::TransferSrc, vk::MemoryUsage::HostOnly);
        VULL_EXPECT(vertex_stream->read({staging_buffer.mapped_raw(), vertex_entry.size}));

        auto &queue = m_context.get_queue(vk::QueueKind::Transfer);
        queue.immediate_submit([&](vk::CommandBuffer &cmd_buf) {
            vkb::BufferCopy copy{
                .dstOffset = vertex_buffer_offset,
                .size = vertex_entry.size,
            };
            cmd_buf.copy_buffer(staging_buffer, m_vertex_buffer, copy);
        });
        vertex_buffer_offset += vertex_entry.size;

        auto index_entry = *vpak::stat(mesh.index_data_name());
        auto index_stream = vpak::open(mesh.index_data_name());
        staging_buffer =
            m_context.create_buffer(index_entry.size, vkb::BufferUsage::TransferSrc, vk::MemoryUsage::HostOnly);
        VULL_EXPECT(index_stream->read({staging_buffer.mapped_raw(), index_entry.size}));

        queue.immediate_submit([&](vk::CommandBuffer &cmd_buf) {
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

void DefaultRenderer::update_ubo(const vk::Buffer &buffer) {
    const auto proj = m_camera->projection_matrix();
    const auto view = m_camera->view_matrix();
    const auto proj_view = proj * view;
    if (!m_cull_view_locked) {
        auto proj_view_t = vull::transpose(proj_view);
        m_frustum_planes[0] = proj_view_t[3] + proj_view_t[0]; // left
        m_frustum_planes[1] = proj_view_t[3] - proj_view_t[0]; // right
        m_frustum_planes[2] = proj_view_t[3] + proj_view_t[1]; // bottom
        m_frustum_planes[3] = proj_view_t[3] - proj_view_t[1]; // top
        for (auto &plane : m_frustum_planes) {
            plane /= vull::magnitude(Vec3f(plane));
        }
        m_cull_view = view;
    }

    UniformBuffer frame_ubo_data{
        .proj = proj,
        .inv_proj = vull::inverse(proj),
        .view = view,
        .proj_view = proj_view,
        .inv_proj_view = vull::inverse(proj_view),
        .cull_view = m_cull_view,
        .view_position = m_camera->position(),
        .object_count = m_object_count,
        .frustum_planes = m_frustum_planes,
    };
    memcpy(buffer.mapped_raw(), &frame_ubo_data, sizeof(UniformBuffer));
}

void DefaultRenderer::record_draws(vk::CommandBuffer &cmd_buf, const vk::Buffer &draw_buffer) {
    cmd_buf.bind_index_buffer(m_index_buffer, vkb::IndexType::Uint32);
    cmd_buf.draw_indexed_indirect_count(draw_buffer, sizeof(uint32_t), draw_buffer, 0, m_object_count, sizeof(DrawCmd));
}

vk::ResourceId DefaultRenderer::build_pass(vk::RenderGraph &graph, GBuffer &gbuffer) {
    Vector<Object> objects;
    for (auto [entity, mesh] : m_scene->world().view<Mesh>()) {
        const auto mesh_info = m_mesh_infos.get(mesh.vertex_data_name());
        if (!mesh_info) {
            continue;
        }

        // TODO: Assuming fallback indices here.
        uint32_t albedo_index = 0;
        uint32_t normal_index = 1;
        if (auto material = entity.try_get<Material>()) {
            albedo_index = m_texture_streamer.ensure_texture(material->albedo_name(), TextureKind::Albedo);
            normal_index = m_texture_streamer.ensure_texture(material->normal_name(), TextureKind::Normal);
        }

        auto bounding_sphere = entity.try_get<BoundingSphere>();
        objects.push({
            .transform = m_scene->get_transform_matrix(entity),
            .center = bounding_sphere ? bounding_sphere->center() : Vec3f(0.0f),
            .radius = bounding_sphere ? bounding_sphere->radius() : FLT_MAX,
            .albedo_index = albedo_index,
            .normal_index = normal_index,
            .index_count = mesh_info->index_count,
            .first_index = mesh_info->index_offset,
            .vertex_offset = static_cast<uint32_t>(mesh_info->vertex_offset),
        });
    }

    // Cap object count just in case.
    m_object_count = vull::min(objects.size(), k_object_limit);

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
    vk::BufferDescription object_buffer_description{
        .size = objects.size_bytes(),
        .usage = vkb::BufferUsage::StorageBuffer,
        .host_accessible = true,
    };
    auto descriptor_buffer_id = graph.new_buffer("default-descriptor-buffer", descriptor_buffer_description);
    auto frame_ubo_id = graph.new_buffer("frame-ubo", frame_ubo_description);
    auto object_buffer_id = graph.new_buffer("object-buffer", object_buffer_description);

    auto &setup_pass = graph.add_pass("setup-frame", vk::PassFlag::Transfer)
                           .write(descriptor_buffer_id)
                           .write(frame_ubo_id)
                           .write(object_buffer_id);
    setup_pass.set_on_execute([=, this, &graph, objects = vull::move(objects)](vk::CommandBuffer &) {
        const auto &frame_ubo = graph.get_buffer(frame_ubo_id);
        update_ubo(frame_ubo);

        const auto &object_buffer = graph.get_buffer(object_buffer_id);
        memcpy(object_buffer.mapped_raw(), objects.data(), objects.size_bytes());

        vk::DescriptorBuilder descriptor_builder(m_main_set_layout, graph.get_buffer(descriptor_buffer_id));
        descriptor_builder.set(0, 0, frame_ubo);
        descriptor_builder.set(1, 0, object_buffer);
        descriptor_builder.set(2, 0, m_object_visibility_buffer);
        descriptor_builder.set(5, 0, m_vertex_buffer);
    });

    vk::BufferDescription draw_buffer_description{
        .size = sizeof(uint32_t) + m_object_count * sizeof(DrawCmd),
        .usage = vkb::BufferUsage::StorageBuffer | vkb::BufferUsage::IndirectBuffer | vkb::BufferUsage::TransferDst,
    };
    auto draw_buffer_id = graph.new_buffer("draw-buffer", draw_buffer_description);
    auto &early_cull_pass =
        graph.add_pass("early-cull", vk::PassFlag::Compute).read(frame_ubo_id).write(draw_buffer_id);
    early_cull_pass.set_on_execute([=, this, &graph](vk::CommandBuffer &cmd_buf) {
        const auto &descriptor_buffer = graph.get_buffer(descriptor_buffer_id);
        const auto &draw_buffer = graph.get_buffer(draw_buffer_id);
        cmd_buf.zero_buffer(draw_buffer, 0, sizeof(uint32_t));
        // TODO: This should be a separate pass so RG can add the barrier itself.
        cmd_buf.buffer_barrier({
            .sType = vkb::StructureType::BufferMemoryBarrier2,
            .srcStageMask = vkb::PipelineStage2::Clear,
            .srcAccessMask = vkb::Access2::TransferWrite,
            .dstStageMask = vkb::PipelineStage2::ComputeShader,
            .dstAccessMask = vkb::Access2::ShaderStorageRead,
            .buffer = *draw_buffer,
            .size = sizeof(uint32_t),
        });

        vk::DescriptorBuilder(m_main_set_layout, descriptor_buffer).set(3, 0, draw_buffer);
        cmd_buf.bind_descriptor_buffer(vkb::PipelineBindPoint::Compute, descriptor_buffer, 0, 0);
        cmd_buf.bind_pipeline(m_early_cull_pipeline);
        cmd_buf.dispatch(vull::ceil_div(m_object_count, 32));
    });

    // TODO: Make GBuffer writes additive.
    auto &early_draw_pass = graph.add_pass("early-draw", vk::PassFlag::Graphics)
                                .read(draw_buffer_id, vk::ReadFlag::Indirect)
                                .write(gbuffer.albedo)
                                .write(gbuffer.normal)
                                .write(gbuffer.depth);
    early_draw_pass.set_on_execute([=, this, &graph](vk::CommandBuffer &cmd_buf) {
        const auto &descriptor_buffer = graph.get_buffer(descriptor_buffer_id);
        const auto &draw_buffer = graph.get_buffer(draw_buffer_id);
        cmd_buf.bind_descriptor_buffer(vkb::PipelineBindPoint::Graphics, descriptor_buffer, 0, 0);
        cmd_buf.bind_descriptor_buffer(vkb::PipelineBindPoint::Graphics, m_texture_streamer.descriptor_buffer(), 1, 0);
        cmd_buf.bind_pipeline(m_gbuffer_pipeline);
        record_draws(cmd_buf, draw_buffer);
    });

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
    auto depth_pyramid_id = graph.new_attachment("depth-pyramid", depth_pyramid_description);
    // TODO: This should be part of DeferredRenderer.
    auto &depth_reduce_pass =
        graph.add_pass("depth-reduce", vk::PassFlag::Compute).read(gbuffer.depth).write(depth_pyramid_id);
    depth_reduce_pass.set_on_execute([=, this, &graph](vk::CommandBuffer &cmd_buf) {
        const auto &depth_image = graph.get_image(gbuffer.depth);
        const auto &depth_pyramid = graph.get_image(depth_pyramid_id);
        const uint32_t level_count = depth_pyramid.full_view().range().levelCount;
        vk::DescriptorBuilder(m_main_set_layout, graph.get_buffer(descriptor_buffer_id))
            .set(4, 0, depth_pyramid.full_view().sampled(vk::Sampler::DepthReduce));

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
            cmd_buf.bind_descriptor_buffer(vkb::PipelineBindPoint::Compute, descriptor_buffer, 0, descriptor_offset);
            descriptor_offset += m_reduce_set_layout_size;

            DepthReduceData shader_data{
                .mip_size{
                    vull::max(m_depth_pyramid_extent.width >> i, 1),
                    vull::max(m_depth_pyramid_extent.height >> i, 1),
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
            cmd_buf.dispatch(vull::ceil_div(shader_data.mip_size.x(), 32),
                             vull::ceil_div(shader_data.mip_size.y(), 32));
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

    auto &late_cull_pass =
        graph.add_pass("late-cull", vk::PassFlag::Compute).read(depth_pyramid_id).write(draw_buffer_id);
    late_cull_pass.set_on_execute([=, this, &graph](vk::CommandBuffer &cmd_buf) {
        const auto &descriptor_buffer = graph.get_buffer(descriptor_buffer_id);
        const auto &draw_buffer = graph.get_buffer(draw_buffer_id);
        // Prevent write-after-read.
        cmd_buf.buffer_barrier({
            .sType = vkb::StructureType::BufferMemoryBarrier2,
            .srcStageMask = vkb::PipelineStage2::DrawIndirect,
            .srcAccessMask = vkb::Access2::IndirectCommandRead,
            .dstStageMask = vkb::PipelineStage2::Clear,
            .dstAccessMask = vkb::Access2::TransferWrite,
            .buffer = *draw_buffer,
            .size = vkb::k_whole_size,
        });
        cmd_buf.zero_buffer(draw_buffer, 0, sizeof(uint32_t));
        cmd_buf.buffer_barrier({
            .sType = vkb::StructureType::BufferMemoryBarrier2,
            .srcStageMask = vkb::PipelineStage2::Clear,
            .srcAccessMask = vkb::Access2::TransferWrite,
            .dstStageMask = vkb::PipelineStage2::ComputeShader,
            .dstAccessMask = vkb::Access2::ShaderStorageRead,
            .buffer = *draw_buffer,
            .size = sizeof(uint32_t),
        });

        cmd_buf.bind_descriptor_buffer(vkb::PipelineBindPoint::Compute, descriptor_buffer, 0, 0);
        cmd_buf.bind_pipeline(m_late_cull_pipeline);
        cmd_buf.dispatch(vull::ceil_div(m_object_count, 32));
    });

    auto &late_draw_pass = graph.add_pass("late-draw", vk::PassFlag::Graphics)
                               .read(draw_buffer_id, vk::ReadFlag::Indirect)
                               .write(gbuffer.albedo, vk::WriteFlag::Additive)
                               .write(gbuffer.normal, vk::WriteFlag::Additive)
                               .write(gbuffer.depth, vk::WriteFlag::Additive);
    late_draw_pass.set_on_execute([=, this, &graph](vk::CommandBuffer &cmd_buf) {
        const auto &descriptor_buffer = graph.get_buffer(descriptor_buffer_id);
        const auto &draw_buffer = graph.get_buffer(draw_buffer_id);
        cmd_buf.bind_descriptor_buffer(vkb::PipelineBindPoint::Graphics, descriptor_buffer, 0, 0);
        cmd_buf.bind_descriptor_buffer(vkb::PipelineBindPoint::Graphics, m_texture_streamer.descriptor_buffer(), 1, 0);
        cmd_buf.bind_pipeline(m_gbuffer_pipeline);
        record_draws(cmd_buf, draw_buffer);
    });
    return frame_ubo_id;
}

} // namespace vull
