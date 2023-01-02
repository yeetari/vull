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
#include <vull/vulkan/Image.hh>
#include <vull/vulkan/ImageView.hh>
#include <vull/vulkan/MemoryUsage.hh>
#include <vull/vulkan/Pipeline.hh>
#include <vull/vulkan/PipelineBuilder.hh>
#include <vull/vulkan/Queue.hh>
#include <vull/vulkan/RenderGraph.hh>
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
    : m_context(context), m_viewport_extent(viewport_extent) {
    m_tile_extent = {
        .width = vull::ceil_div(m_viewport_extent.width, k_tile_size),
        .height = vull::ceil_div(m_viewport_extent.height, k_tile_size),
    };
    create_set_layouts();
    create_resources();
    create_pipelines(vull::move(shader_map));
    create_render_graph();
}

DefaultRenderer::~DefaultRenderer() {
    m_context.vkDestroySampler(m_shadow_sampler);
    m_context.vkDestroySampler(m_depth_reduce_sampler);
    m_context.vkDestroyDescriptorSetLayout(m_reduce_set_layout);
    m_context.vkDestroyDescriptorSetLayout(m_texture_set_layout);
    m_context.vkDestroyDescriptorSetLayout(m_dynamic_set_layout);
    m_context.vkDestroyDescriptorSetLayout(m_static_set_layout);
}

void DefaultRenderer::create_set_layouts() {
    Array static_set_bindings{
        vkb::DescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = vkb::DescriptorType::SampledImage,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Compute,
        },
        vkb::DescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = vkb::DescriptorType::SampledImage,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Compute,
        },
        vkb::DescriptorSetLayoutBinding{
            .binding = 2,
            .descriptorType = vkb::DescriptorType::SampledImage,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Compute,
        },
        vkb::DescriptorSetLayoutBinding{
            .binding = 3,
            .descriptorType = vkb::DescriptorType::CombinedImageSampler,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Compute,
        },
        vkb::DescriptorSetLayoutBinding{
            .binding = 4,
            .descriptorType = vkb::DescriptorType::CombinedImageSampler,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Compute,
        },
        vkb::DescriptorSetLayoutBinding{
            .binding = 5,
            .descriptorType = vkb::DescriptorType::StorageBuffer,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Compute,
        },
        vkb::DescriptorSetLayoutBinding{
            .binding = 6,
            .descriptorType = vkb::DescriptorType::StorageBuffer,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Compute,
        },
    };
    vkb::DescriptorSetLayoutCreateInfo static_set_layout_ci{
        .sType = vkb::StructureType::DescriptorSetLayoutCreateInfo,
        .flags = vkb::DescriptorSetLayoutCreateFlags::DescriptorBufferEXT,
        .bindingCount = static_set_bindings.size(),
        .pBindings = static_set_bindings.data(),
    };
    VULL_ENSURE(m_context.vkCreateDescriptorSetLayout(&static_set_layout_ci, &m_static_set_layout) ==
                vkb::Result::Success);

    Array dynamic_set_bindings{
        vkb::DescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = vkb::DescriptorType::UniformBuffer,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::All,
        },
        vkb::DescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = vkb::DescriptorType::StorageBuffer,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Compute,
        },
        vkb::DescriptorSetLayoutBinding{
            .binding = 2,
            .descriptorType = vkb::DescriptorType::StorageImage,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Compute,
        },
        vkb::DescriptorSetLayoutBinding{
            .binding = 3,
            .descriptorType = vkb::DescriptorType::StorageBuffer,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Vertex | vkb::ShaderStage::Compute,
        },
        vkb::DescriptorSetLayoutBinding{
            .binding = 4,
            .descriptorType = vkb::DescriptorType::StorageBuffer,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Vertex | vkb::ShaderStage::Compute,
        },
    };
    vkb::DescriptorSetLayoutCreateInfo dynamic_set_layout_ci{
        .sType = vkb::StructureType::DescriptorSetLayoutCreateInfo,
        .flags = vkb::DescriptorSetLayoutCreateFlags::DescriptorBufferEXT,
        .bindingCount = dynamic_set_bindings.size(),
        .pBindings = dynamic_set_bindings.data(),
    };
    VULL_ENSURE(m_context.vkCreateDescriptorSetLayout(&dynamic_set_layout_ci, &m_dynamic_set_layout) ==
                vkb::Result::Success);

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

    m_context.vkGetDescriptorSetLayoutSizeEXT(m_static_set_layout, &m_static_set_layout_size);
    m_context.vkGetDescriptorSetLayoutSizeEXT(m_dynamic_set_layout, &m_dynamic_set_layout_size);
    m_context.vkGetDescriptorSetLayoutSizeEXT(m_texture_set_layout, &m_texture_set_layout_size);
    m_context.vkGetDescriptorSetLayoutSizeEXT(m_reduce_set_layout, &m_reduce_set_layout_size);
}

void DefaultRenderer::create_resources() {
    vkb::ImageCreateInfo albedo_image_ci{
        .sType = vkb::StructureType::ImageCreateInfo,
        .imageType = vkb::ImageType::_2D,
        .format = vkb::Format::R8G8B8A8Unorm,
        .extent = m_viewport_extent,
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = vkb::SampleCount::_1,
        .tiling = vkb::ImageTiling::Optimal,
        .usage = vkb::ImageUsage::ColorAttachment | vkb::ImageUsage::Sampled,
        .sharingMode = vkb::SharingMode::Exclusive,
        .initialLayout = vkb::ImageLayout::Undefined,
    };
    m_albedo_image = m_context.create_image(albedo_image_ci, vk::MemoryUsage::DeviceOnly);

    vkb::ImageCreateInfo normal_image_ci{
        .sType = vkb::StructureType::ImageCreateInfo,
        .imageType = vkb::ImageType::_2D,
        .format = vkb::Format::R32G32B32A32Sfloat,
        .extent = m_viewport_extent,
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = vkb::SampleCount::_1,
        .tiling = vkb::ImageTiling::Optimal,
        .usage = vkb::ImageUsage::ColorAttachment | vkb::ImageUsage::Sampled,
        .sharingMode = vkb::SharingMode::Exclusive,
        .initialLayout = vkb::ImageLayout::Undefined,
    };
    m_normal_image = m_context.create_image(normal_image_ci, vk::MemoryUsage::DeviceOnly);

    vkb::ImageCreateInfo depth_image_ci{
        .sType = vkb::StructureType::ImageCreateInfo,
        .imageType = vkb::ImageType::_2D,
        .format = vkb::Format::D32Sfloat,
        .extent = m_viewport_extent,
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = vkb::SampleCount::_1,
        .tiling = vkb::ImageTiling::Optimal,
        .usage = vkb::ImageUsage::DepthStencilAttachment | vkb::ImageUsage::Sampled,
        .sharingMode = vkb::SharingMode::Exclusive,
        .initialLayout = vkb::ImageLayout::Undefined,
    };
    m_depth_image = m_context.create_image(depth_image_ci, vk::MemoryUsage::DeviceOnly);

    // Round down viewport to previous power of two.
    m_depth_pyramid_extent = {1u << vull::log2(m_viewport_extent.width), 1u << vull::log2(m_viewport_extent.height)};
    const auto depth_pyramid_mip_count =
        vull::log2(vull::max(m_depth_pyramid_extent.width, m_depth_pyramid_extent.height)) + 1;
    vkb::ImageCreateInfo depth_pyramid_image_ci{
        .sType = vkb::StructureType::ImageCreateInfo,
        .imageType = vkb::ImageType::_2D,
        .format = vkb::Format::R16Sfloat,
        .extent = {m_depth_pyramid_extent.width, m_depth_pyramid_extent.height, 1},
        .mipLevels = depth_pyramid_mip_count,
        .arrayLayers = 1,
        .samples = vkb::SampleCount::_1,
        .tiling = vkb::ImageTiling::Optimal,
        .usage = vkb::ImageUsage::Storage | vkb::ImageUsage::Sampled,
        .sharingMode = vkb::SharingMode::Exclusive,
        .initialLayout = vkb::ImageLayout::Undefined,
    };
    m_depth_pyramid_image = m_context.create_image(depth_pyramid_image_ci, vk::MemoryUsage::DeviceOnly);
    for (uint32_t i = 0; i < depth_pyramid_mip_count; i++) {
        m_depth_pyramid_views.push(
            m_depth_pyramid_image.create_level_view(i, vkb::ImageUsage::Storage | vkb::ImageUsage::Sampled));
    }
    vkb::SamplerReductionModeCreateInfo depth_reduction_mode_ci{
        .sType = vkb::StructureType::SamplerReductionModeCreateInfo,
        .reductionMode = vkb::SamplerReductionMode::Min,
    };
    vkb::SamplerCreateInfo depth_reduce_sampler_ci{
        .sType = vkb::StructureType::SamplerCreateInfo,
        .pNext = &depth_reduction_mode_ci,
        .magFilter = vkb::Filter::Linear,
        .minFilter = vkb::Filter::Linear,
        .mipmapMode = vkb::SamplerMipmapMode::Nearest,
        .addressModeU = vkb::SamplerAddressMode::ClampToEdge,
        .addressModeV = vkb::SamplerAddressMode::ClampToEdge,
        .addressModeW = vkb::SamplerAddressMode::ClampToEdge,
        .maxLod = vkb::k_lod_clamp_none,
    };
    VULL_ENSURE(m_context.vkCreateSampler(&depth_reduce_sampler_ci, &m_depth_reduce_sampler) == vkb::Result::Success);

    vkb::ImageCreateInfo shadow_map_image_ci{
        .sType = vkb::StructureType::ImageCreateInfo,
        .imageType = vkb::ImageType::_2D,
        .format = vkb::Format::D32Sfloat,
        .extent = {k_shadow_resolution, k_shadow_resolution, 1},
        .mipLevels = 1,
        .arrayLayers = k_cascade_count,
        .samples = vkb::SampleCount::_1,
        .tiling = vkb::ImageTiling::Optimal,
        .usage = vkb::ImageUsage::DepthStencilAttachment | vkb::ImageUsage::Sampled,
        .sharingMode = vkb::SharingMode::Exclusive,
        .initialLayout = vkb::ImageLayout::Undefined,
    };
    m_shadow_map_image = m_context.create_image(shadow_map_image_ci, vk::MemoryUsage::DeviceOnly);
    for (uint32_t i = 0; i < k_cascade_count; i++) {
        m_shadow_cascade_views.push(m_shadow_map_image.create_layer_view(i, vkb::ImageUsage::Sampled));
    }
    vkb::SamplerCreateInfo shadow_sampler_ci{
        .sType = vkb::StructureType::SamplerCreateInfo,
        .magFilter = vkb::Filter::Linear,
        .minFilter = vkb::Filter::Linear,
        .mipmapMode = vkb::SamplerMipmapMode::Linear,
        .addressModeU = vkb::SamplerAddressMode::ClampToEdge,
        .addressModeV = vkb::SamplerAddressMode::ClampToEdge,
        .addressModeW = vkb::SamplerAddressMode::ClampToEdge,
        .compareEnable = true,
        .compareOp = vkb::CompareOp::Less,
        .borderColor = vkb::BorderColor::FloatOpaqueWhite,
    };
    VULL_ENSURE(m_context.vkCreateSampler(&shadow_sampler_ci, &m_shadow_sampler) == vkb::Result::Success);

    const auto light_visibility_buffer_size =
        (sizeof(uint32_t) + k_tile_max_light_count * sizeof(uint32_t)) * m_tile_extent.width * m_tile_extent.height;
    m_light_visibility_buffer = m_context.create_buffer(light_visibility_buffer_size, vkb::BufferUsage::StorageBuffer,
                                                        vk::MemoryUsage::DeviceOnly);

    m_object_visibility_buffer = m_context.create_buffer(
        (k_object_limit * sizeof(uint32_t)) / 32, vkb::BufferUsage::StorageBuffer | vkb::BufferUsage::TransferDst,
        vk::MemoryUsage::DeviceOnly);
    m_context.graphics_queue().immediate_submit([this](vk::CommandBuffer &cmd_buf) {
        m_context.vkCmdFillBuffer(*cmd_buf, *m_object_visibility_buffer, 0, m_object_visibility_buffer.size(), 0);
    });

    m_static_descriptor_buffer =
        m_context.create_buffer(m_static_set_layout_size,
                                vkb::BufferUsage::SamplerDescriptorBufferEXT |
                                    vkb::BufferUsage::ResourceDescriptorBufferEXT | vkb::BufferUsage::TransferDst,
                                vk::MemoryUsage::DeviceOnly);

    vkb::DescriptorImageInfo albedo_image_info{
        .imageView = *m_albedo_image.full_view(),
        .imageLayout = vkb::ImageLayout::ReadOnlyOptimal,
    };
    vkb::DescriptorImageInfo normal_image_info{
        .imageView = *m_normal_image.full_view(),
        .imageLayout = vkb::ImageLayout::ReadOnlyOptimal,
    };
    vkb::DescriptorImageInfo depth_image_info{
        .imageView = *m_depth_image.full_view(),
        .imageLayout = vkb::ImageLayout::ReadOnlyOptimal,
    };
    vkb::DescriptorImageInfo depth_pyramid_image_info{
        .sampler = m_depth_reduce_sampler,
        .imageView = *m_depth_pyramid_image.full_view(),
        .imageLayout = vkb::ImageLayout::ReadOnlyOptimal,
    };
    vkb::DescriptorImageInfo shadow_map_image_info{
        .sampler = m_shadow_sampler,
        .imageView = *m_shadow_map_image.full_view(),
        .imageLayout = vkb::ImageLayout::ReadOnlyOptimal,
    };
    vkb::DescriptorAddressInfoEXT light_visibility_buffer_ai{
        .sType = vkb::StructureType::DescriptorAddressInfoEXT,
        .address = m_light_visibility_buffer.device_address(),
        .range = m_light_visibility_buffer.size(),
    };
    vkb::DescriptorAddressInfoEXT object_visibility_buffer_ai{
        .sType = vkb::StructureType::DescriptorAddressInfoEXT,
        .address = m_object_visibility_buffer.device_address(),
        .range = m_object_visibility_buffer.size(),
    };
    auto staging_buffer = m_static_descriptor_buffer.create_staging();
    auto *descriptor_data = staging_buffer.mapped<uint8_t>();
    descriptor_data = put_descriptor(descriptor_data, vkb::DescriptorType::SampledImage, &albedo_image_info);
    descriptor_data = put_descriptor(descriptor_data, vkb::DescriptorType::SampledImage, &normal_image_info);
    descriptor_data = put_descriptor(descriptor_data, vkb::DescriptorType::SampledImage, &depth_image_info);
    descriptor_data =
        put_descriptor(descriptor_data, vkb::DescriptorType::CombinedImageSampler, &depth_pyramid_image_info);
    descriptor_data =
        put_descriptor(descriptor_data, vkb::DescriptorType::CombinedImageSampler, &shadow_map_image_info);
    descriptor_data = put_descriptor(descriptor_data, vkb::DescriptorType::StorageBuffer, &light_visibility_buffer_ai);
    descriptor_data = put_descriptor(descriptor_data, vkb::DescriptorType::StorageBuffer, &object_visibility_buffer_ai);
    m_static_descriptor_buffer.copy_from(staging_buffer, m_context.graphics_queue());
}

void DefaultRenderer::create_pipelines(ShaderMap &&shader_map) {
    struct SpecializationData {
        uint32_t viewport_width;
        uint32_t viewport_height;
        uint32_t tile_size;
        uint32_t tile_max_light_count;
        uint32_t row_tile_count;
    } specialization_data{
        .viewport_width = m_viewport_extent.width,
        .viewport_height = m_viewport_extent.height,
        .tile_size = k_tile_size,
        .tile_max_light_count = k_tile_max_light_count,
        .row_tile_count = m_tile_extent.width,
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
                             .add_colour_attachment(m_albedo_image.format())
                             .add_colour_attachment(m_normal_image.format())
                             .add_set_layout(m_dynamic_set_layout)
                             .add_set_layout(m_texture_set_layout)
                             .add_shader(*shader_map.get("gbuffer-vert"), specialization_info)
                             .add_shader(*shader_map.get("gbuffer-frag"), specialization_info)
                             .add_vertex_attribute(vkb::Format::R32G32B32Sfloat, offsetof(Vertex, position))
                             .add_vertex_attribute(vkb::Format::R32G32B32Sfloat, offsetof(Vertex, normal))
                             .add_vertex_attribute(vkb::Format::R32G32Sfloat, offsetof(Vertex, uv))
                             .set_cull_mode(vkb::CullMode::Back, vkb::FrontFace::CounterClockwise)
                             .set_depth_format(m_depth_image.format())
                             .set_depth_params(vkb::CompareOp::GreaterOrEqual, true, true)
                             .set_topology(vkb::PrimitiveTopology::TriangleList)
                             .set_vertex_binding(sizeof(Vertex))
                             .set_viewport(m_viewport_extent)
                             .build(m_context);

    m_shadow_pipeline = vk::PipelineBuilder()
                            .add_set_layout(m_dynamic_set_layout)
                            .add_shader(*shader_map.get("shadow"), specialization_info)
                            .add_vertex_attribute(vkb::Format::R32G32B32Sfloat, offsetof(Vertex, position))
                            .set_cull_mode(vkb::CullMode::Back, vkb::FrontFace::CounterClockwise)
                            .set_depth_bias(2.0f, 5.0f)
                            .set_depth_format(vkb::Format::D32Sfloat)
                            .set_depth_params(vkb::CompareOp::LessOrEqual, true, true)
                            .set_push_constant_range({
                                .stageFlags = vkb::ShaderStage::Vertex,
                                .size = sizeof(ShadowPushConstantBlock),
                            })
                            .set_topology(vkb::PrimitiveTopology::TriangleList)
                            .set_vertex_binding(sizeof(Vertex))
                            .set_viewport(vkb::Extent2D{k_shadow_resolution, k_shadow_resolution})
                            .build(m_context);

    m_depth_reduce_pipeline = vk::PipelineBuilder()
                                  .add_set_layout(m_reduce_set_layout)
                                  .add_shader(*shader_map.get("depth-reduce"))
                                  .set_push_constant_range({
                                      .stageFlags = vkb::ShaderStage::Compute,
                                      .size = sizeof(DepthReduceData),
                                  })
                                  .build(m_context);

    m_early_cull_pipeline = vk::PipelineBuilder()
                                .add_set_layout(m_dynamic_set_layout)
                                .add_set_layout(m_static_set_layout)
                                .add_shader(*shader_map.get("draw-cull"))
                                .build(m_context);

    m_late_cull_pipeline = vk::PipelineBuilder()
                               .add_set_layout(m_dynamic_set_layout)
                               .add_set_layout(m_static_set_layout)
                               .add_shader(*shader_map.get("draw-cull"), late_specialization_info)
                               .build(m_context);

    m_light_cull_pipeline = vk::PipelineBuilder()
                                .add_set_layout(m_dynamic_set_layout)
                                .add_set_layout(m_static_set_layout)
                                .add_shader(*shader_map.get("light-cull"), specialization_info)
                                .build(m_context);

    m_deferred_pipeline = vk::PipelineBuilder()
                              .add_set_layout(m_dynamic_set_layout)
                              .add_set_layout(m_static_set_layout)
                              .add_shader(*shader_map.get("deferred"), specialization_info)
                              .build(m_context);
}

void DefaultRenderer::create_render_graph() {
    auto &albedo_image_resource = m_render_graph.add_image("gbuffer-albedo");
    auto &normal_image_resource = m_render_graph.add_image("gbuffer-normal");
    auto &depth_image_resource = m_render_graph.add_image("gbuffer-depth");
    auto &depth_pyramid_resource = m_render_graph.add_image("depth-pyramid");
    auto &shadow_map_resource = m_render_graph.add_image("shadow-map");
    auto &light_visibility_buffer_resource = m_render_graph.add_storage_buffer("light-visibility-buffer");
    auto &object_visibility_buffer_resource = m_render_graph.add_storage_buffer("object-visibility-buffer");
    albedo_image_resource.set(m_albedo_image.full_view());
    normal_image_resource.set(m_normal_image.full_view());
    depth_image_resource.set(m_depth_image.full_view());
    depth_pyramid_resource.set(m_depth_pyramid_image.full_view());
    shadow_map_resource.set(m_shadow_map_image.full_view());
    light_visibility_buffer_resource.set(m_light_visibility_buffer);
    object_visibility_buffer_resource.set(m_object_visibility_buffer);

    m_uniform_buffer_resource = &m_render_graph.add_uniform_buffer("global-ubo");
    m_light_buffer_resource = &m_render_graph.add_storage_buffer("light-buffer");
    m_early_draw_buffer_resource = &m_render_graph.add_storage_buffer("draw-buffer-1");
    m_late_draw_buffer_resource = &m_render_graph.add_storage_buffer("draw-buffer-2");
    m_output_image_resource = &m_render_graph.add_image("output-image");
    m_output_image_resource->set_range({
        .aspectMask = vkb::ImageAspect::Color,
        .levelCount = 1,
        .layerCount = 1,
    });

    // TODO: object_visibility_buffer_resource should be read from early_cull_pass and written from late_cull_pass, but
    //       since the render graph isn't interframe then it wouldn't be correct. Sync is currently fine because of the
    //       big frame barrier.

    auto &early_cull_pass = m_render_graph.add_compute_pass("early-cull");
    early_cull_pass.reads_from(*m_uniform_buffer_resource);
    early_cull_pass.writes_to(*m_early_draw_buffer_resource);
    early_cull_pass.set_on_record([this](vk::CommandBuffer &cmd_buf) {
        m_context.vkCmdFillBuffer(*cmd_buf, *m_draw_buffer, 0, sizeof(uint32_t), 0);
        cmd_buf.bind_descriptor_buffer(vkb::PipelineBindPoint::Compute, m_dynamic_descriptor_buffer, 0, 0);
        cmd_buf.bind_descriptor_buffer(vkb::PipelineBindPoint::Compute, m_static_descriptor_buffer, 1, 0);
        cmd_buf.bind_pipeline(m_early_cull_pipeline);
        cmd_buf.dispatch(vull::ceil_div(m_object_count, 32u));
    });

    auto &early_draw_pass = m_render_graph.add_graphics_pass("early-draw");
    early_draw_pass.reads_from(*m_uniform_buffer_resource);
    early_draw_pass.reads_from(*m_early_draw_buffer_resource);
    early_draw_pass.writes_to(albedo_image_resource);
    early_draw_pass.writes_to(normal_image_resource);
    early_draw_pass.writes_to(depth_image_resource);
    early_draw_pass.set_on_record([this](vk::CommandBuffer &cmd_buf) {
        cmd_buf.bind_descriptor_buffer(vkb::PipelineBindPoint::Graphics, m_dynamic_descriptor_buffer, 0, 0);
        cmd_buf.bind_descriptor_buffer(vkb::PipelineBindPoint::Graphics, m_texture_descriptor_buffer, 1, 0);
        Array colour_write_attachments{
            vkb::RenderingAttachmentInfo{
                .sType = vkb::StructureType::RenderingAttachmentInfo,
                .imageView = *m_albedo_image.full_view(),
                .imageLayout = vkb::ImageLayout::ColorAttachmentOptimal,
                .loadOp = vkb::AttachmentLoadOp::Clear,
                .storeOp = vkb::AttachmentStoreOp::Store,
                .clearValue{
                    .color{{0.0f, 0.0f, 0.0f, 0.0f}},
                },
            },
            vkb::RenderingAttachmentInfo{
                .sType = vkb::StructureType::RenderingAttachmentInfo,
                .imageView = *m_normal_image.full_view(),
                .imageLayout = vkb::ImageLayout::ColorAttachmentOptimal,
                .loadOp = vkb::AttachmentLoadOp::Clear,
                .storeOp = vkb::AttachmentStoreOp::Store,
                .clearValue{
                    .color{{0.0f, 0.0f, 0.0f, 0.0f}},
                },
            },
        };
        vkb::RenderingAttachmentInfo depth_write_attachment{
            .sType = vkb::StructureType::RenderingAttachmentInfo,
            .imageView = *m_depth_image.full_view(),
            .imageLayout = vkb::ImageLayout::DepthAttachmentOptimal,
            .loadOp = vkb::AttachmentLoadOp::Clear,
            .storeOp = vkb::AttachmentStoreOp::Store,
            .clearValue{
                .depthStencil{0.0f, 0},
            },
        };
        vkb::RenderingInfo rendering_info{
            .sType = vkb::StructureType::RenderingInfo,
            .renderArea{
                .extent = {m_viewport_extent.width, m_viewport_extent.height},
            },
            .layerCount = 1,
            .colorAttachmentCount = colour_write_attachments.size(),
            .pColorAttachments = colour_write_attachments.data(),
            .pDepthAttachment = &depth_write_attachment,
        };
        cmd_buf.bind_pipeline(m_gbuffer_pipeline);
        cmd_buf.begin_rendering(rendering_info);
        record_draws(cmd_buf);
        cmd_buf.end_rendering();
    });

    auto &depth_reduce_pass = m_render_graph.add_compute_pass("depth-reduce");
    depth_reduce_pass.reads_from(depth_image_resource);
    depth_reduce_pass.writes_to(depth_pyramid_resource);
    depth_reduce_pass.set_on_record([this](vk::CommandBuffer &cmd_buf) {
        // TODO(rg-conditional)
        if (m_cull_view_locked) {
            return;
        }

        const uint32_t level_count = m_depth_pyramid_image.full_view().range().levelCount;
        auto descriptor_buffer =
            m_context.create_buffer(m_reduce_set_layout_size * level_count,
                                    vkb::BufferUsage::SamplerDescriptorBufferEXT, vk::MemoryUsage::HostToDevice);
        auto *descriptor_data = descriptor_buffer.mapped<uint8_t>();

        cmd_buf.bind_pipeline(m_depth_reduce_pipeline);
        for (uint32_t i = 0; i < level_count; i++) {
            const auto descriptor_offset =
                static_cast<vkb::DeviceSize>(descriptor_data - descriptor_buffer.mapped<uint8_t>());
            cmd_buf.bind_descriptor_buffer(vkb::PipelineBindPoint::Compute, descriptor_buffer, 0, descriptor_offset);

            DepthReduceData shader_data{
                .mip_size{
                    vull::max(m_depth_pyramid_extent.width >> i, 1u),
                    vull::max(m_depth_pyramid_extent.height >> i, 1u),
                },
            };
            cmd_buf.push_constants(vkb::ShaderStage::Compute, shader_data);

            // Update descriptors.
            vkb::DescriptorImageInfo input_image_info{
                .sampler = m_depth_reduce_sampler,
                .imageView = i != 0 ? *m_depth_pyramid_views[i - 1] : *m_depth_image.full_view(),
                .imageLayout = vkb::ImageLayout::ReadOnlyOptimal,
            };
            vkb::DescriptorImageInfo output_image_info{
                .imageView = *m_depth_pyramid_views[i],
                .imageLayout = vkb::ImageLayout::General,
            };
            descriptor_data =
                put_descriptor(descriptor_data, vkb::DescriptorType::CombinedImageSampler, &input_image_info);
            descriptor_data = put_descriptor(descriptor_data, vkb::DescriptorType::StorageImage, &output_image_info);

            vkb::ImageMemoryBarrier2 sample_barrier{
                .sType = vkb::StructureType::ImageMemoryBarrier2,
                .srcStageMask = vkb::PipelineStage2::ComputeShader,
                .srcAccessMask = vkb::Access2::ShaderStorageWrite,
                .dstStageMask = vkb::PipelineStage2::ComputeShader,
                .dstAccessMask = vkb::Access2::ShaderSampledRead,
                .oldLayout = vkb::ImageLayout::General,
                .newLayout = vkb::ImageLayout::ReadOnlyOptimal,
                .image = *m_depth_pyramid_image,
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

        // TODO: Since we transitioned each mip manually to ReadOnlyOptimal, the render graph doesn't know, so we must
        //       transition it back to General.
        vkb::ImageMemoryBarrier2 general_barrier{
            .sType = vkb::StructureType::ImageMemoryBarrier2,
            .srcStageMask = vkb::PipelineStage2::ComputeShader,
            .srcAccessMask = vkb::Access2::ShaderStorageWrite,
            .dstStageMask = vkb::PipelineStage2::AllCommands,
            .dstAccessMask = vkb::Access2::ShaderSampledRead,
            .oldLayout = vkb::ImageLayout::ReadOnlyOptimal,
            .newLayout = vkb::ImageLayout::General,
            .image = *m_depth_pyramid_image,
            .subresourceRange = m_depth_pyramid_image.full_view().range(),
        };
        cmd_buf.image_barrier(general_barrier);
    });

    auto &late_cull_pass = m_render_graph.add_compute_pass("late-cull");
    late_cull_pass.reads_from(*m_uniform_buffer_resource);
    late_cull_pass.reads_from(depth_pyramid_resource);
    late_cull_pass.writes_to(*m_late_draw_buffer_resource);
    late_cull_pass.set_on_record([this](vk::CommandBuffer &cmd_buf) {
        m_context.vkCmdFillBuffer(*cmd_buf, *m_draw_buffer, 0, sizeof(uint32_t), 0);
        cmd_buf.bind_descriptor_buffer(vkb::PipelineBindPoint::Compute, m_dynamic_descriptor_buffer, 0, 0);
        cmd_buf.bind_descriptor_buffer(vkb::PipelineBindPoint::Compute, m_static_descriptor_buffer, 1, 0);
        cmd_buf.bind_pipeline(m_late_cull_pipeline);
        cmd_buf.dispatch(vull::ceil_div(m_object_count, 32u));
    });

    auto &late_draw_pass = m_render_graph.add_graphics_pass("late-draw");
    late_draw_pass.reads_from(*m_uniform_buffer_resource);
    late_draw_pass.reads_from(*m_late_draw_buffer_resource);
    late_draw_pass.writes_to(albedo_image_resource);
    late_draw_pass.writes_to(normal_image_resource);
    late_draw_pass.writes_to(depth_image_resource);
    late_draw_pass.set_on_record([this](vk::CommandBuffer &cmd_buf) {
        cmd_buf.bind_descriptor_buffer(vkb::PipelineBindPoint::Graphics, m_dynamic_descriptor_buffer, 0, 0);
        cmd_buf.bind_descriptor_buffer(vkb::PipelineBindPoint::Graphics, m_texture_descriptor_buffer, 1, 0);
        Array colour_write_attachments{
            vkb::RenderingAttachmentInfo{
                .sType = vkb::StructureType::RenderingAttachmentInfo,
                .imageView = *m_albedo_image.full_view(),
                .imageLayout = vkb::ImageLayout::ColorAttachmentOptimal,
                .loadOp = vkb::AttachmentLoadOp::Load,
                .storeOp = vkb::AttachmentStoreOp::Store,
                .clearValue{
                    .color{{0.0f, 0.0f, 0.0f, 0.0f}},
                },
            },
            vkb::RenderingAttachmentInfo{
                .sType = vkb::StructureType::RenderingAttachmentInfo,
                .imageView = *m_normal_image.full_view(),
                .imageLayout = vkb::ImageLayout::ColorAttachmentOptimal,
                .loadOp = vkb::AttachmentLoadOp::Load,
                .storeOp = vkb::AttachmentStoreOp::Store,
                .clearValue{
                    .color{{0.0f, 0.0f, 0.0f, 0.0f}},
                },
            },
        };
        vkb::RenderingAttachmentInfo depth_write_attachment{
            .sType = vkb::StructureType::RenderingAttachmentInfo,
            .imageView = *m_depth_image.full_view(),
            .imageLayout = vkb::ImageLayout::DepthAttachmentOptimal,
            .loadOp = vkb::AttachmentLoadOp::Load,
            .storeOp = vkb::AttachmentStoreOp::Store,
            .clearValue{
                .depthStencil{0.0f, 0},
            },
        };
        vkb::RenderingInfo rendering_info{
            .sType = vkb::StructureType::RenderingInfo,
            .renderArea{
                .extent = {m_viewport_extent.width, m_viewport_extent.height},
            },
            .layerCount = 1,
            .colorAttachmentCount = colour_write_attachments.size(),
            .pColorAttachments = colour_write_attachments.data(),
            .pDepthAttachment = &depth_write_attachment,
        };
        cmd_buf.bind_pipeline(m_gbuffer_pipeline);
        cmd_buf.begin_rendering(rendering_info);
        record_draws(cmd_buf);
        cmd_buf.end_rendering();
    });

#if 0
    auto &shadow_pass = m_render_graph.add_graphics_pass("shadow-pass");
    shadow_pass.reads_from(*m_uniform_buffer_resource);
    shadow_pass.reads_from(*m_draw_buffer_resource);
    shadow_pass.writes_to(shadow_map_resource);
    shadow_pass.set_on_record([this](vk::CommandBuffer &cmd_buf) {
        cmd_buf.bind_pipeline(m_shadow_pipeline);
        for (uint32_t i = 0; i < k_cascade_count; i++) {
            vkb::RenderingAttachmentInfo shadow_map_write_attachment{
                .sType = vkb::StructureType::RenderingAttachmentInfo,
                .imageView = *m_shadow_cascade_views[i],
                .imageLayout = vkb::ImageLayout::DepthAttachmentOptimal,
                .loadOp = vkb::AttachmentLoadOp::Clear,
                .storeOp = vkb::AttachmentStoreOp::Store,
                .clearValue{
                    .depthStencil{1.0f, 0},
                },
            };
            vkb::RenderingInfo rendering_info{
                .sType = vkb::StructureType::RenderingInfo,
                .renderArea{
                    .extent = {k_shadow_resolution, k_shadow_resolution},
                },
                .layerCount = 1,
                .pDepthAttachment = &shadow_map_write_attachment,
            };
            ShadowPushConstantBlock push_constant_block{
                .cascade_index = i,
            };
            cmd_buf.begin_rendering(rendering_info);
            cmd_buf.push_constants(vkb::ShaderStage::Vertex, push_constant_block);
            record_draws(cmd_buf);
            cmd_buf.end_rendering();
        }
    });
#endif

    auto &light_cull_pass = m_render_graph.add_compute_pass("light-cull");
    light_cull_pass.reads_from(*m_uniform_buffer_resource);
    light_cull_pass.reads_from(*m_light_buffer_resource);
    light_cull_pass.reads_from(depth_image_resource);
    light_cull_pass.writes_to(light_visibility_buffer_resource);
    light_cull_pass.set_on_record([this](vk::CommandBuffer &cmd_buf) {
        cmd_buf.bind_descriptor_buffer(vkb::PipelineBindPoint::Compute, m_dynamic_descriptor_buffer, 0, 0);
        cmd_buf.bind_descriptor_buffer(vkb::PipelineBindPoint::Compute, m_static_descriptor_buffer, 1, 0);
        cmd_buf.bind_pipeline(m_light_cull_pipeline);
        cmd_buf.dispatch(m_tile_extent.width, m_tile_extent.height);
    });

    auto &deferred_pass = m_render_graph.add_compute_pass("deferred-pass");
    deferred_pass.reads_from(*m_uniform_buffer_resource);
    deferred_pass.reads_from(*m_light_buffer_resource);
    deferred_pass.reads_from(albedo_image_resource);
    deferred_pass.reads_from(normal_image_resource);
    deferred_pass.reads_from(depth_image_resource);
    deferred_pass.reads_from(depth_pyramid_resource);
    deferred_pass.reads_from(shadow_map_resource);
    deferred_pass.reads_from(light_visibility_buffer_resource);
    deferred_pass.writes_to(*m_output_image_resource);
    deferred_pass.set_on_record([this](vk::CommandBuffer &cmd_buf) {
        cmd_buf.bind_pipeline(m_deferred_pipeline);
        cmd_buf.dispatch(vull::ceil_div(m_viewport_extent.width, 8u), vull::ceil_div(m_viewport_extent.height, 8u));
    });
}

uint8_t *DefaultRenderer::put_descriptor(uint8_t *dst, vkb::DescriptorType type, void *info) {
    const auto size = m_context.descriptor_size(type);
    vkb::DescriptorGetInfoEXT get_info{
        .sType = vkb::StructureType::DescriptorGetInfoEXT,
        .type = type,
        .data{
            .pSampler = static_cast<vkb::Sampler *>(info),
        },
    };
    m_context.vkGetDescriptorEXT(&get_info, size, dst);
    return dst + size;
}

void DefaultRenderer::record_draws(vk::CommandBuffer &cmd_buf) {
    cmd_buf.bind_vertex_buffer(m_vertex_buffer);
    cmd_buf.bind_index_buffer(m_index_buffer, vkb::IndexType::Uint32);
    cmd_buf.draw_indexed_indirect_count(m_draw_buffer, sizeof(uint32_t), m_draw_buffer, 0, m_object_count,
                                        sizeof(DrawCmd));
}

void DefaultRenderer::update_buffers(vk::CommandBuffer &cmd_buf) {
    Array lights{
        PointLight{
            .position = Vec3f(0.0f),
            .radius = 1.0f,
            .colour = Vec3f(1.0f),
        },
    };

    auto uniform_buffer =
        m_context.create_buffer(sizeof(UniformBuffer), vkb::BufferUsage::UniformBuffer, vk::MemoryUsage::HostToDevice);
    m_uniform_buffer_resource->set(uniform_buffer);

    // TODO: Light buffer may benefit from being DeviceOnly and transferred to.
    auto light_buffer = m_context.create_buffer(lights.size_bytes() + 4 * sizeof(float),
                                                vkb::BufferUsage::UniformBuffer, vk::MemoryUsage::HostToDevice);
    m_light_buffer_resource->set(light_buffer);
    uint32_t light_count = lights.size();
    memcpy(light_buffer.mapped_raw(), &light_count, sizeof(uint32_t));
    memcpy(light_buffer.mapped<uint8_t>() + 4 * sizeof(float), lights.data(), lights.size_bytes());

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
    *uniform_buffer.mapped<UniformBuffer>() = {
        .proj = m_proj,
        .cull_view = m_cull_view,
        .view = m_view,
        .view_position = m_view_position,
        .object_count = m_object_count,
        .frustum_planes = m_frustum_planes,
        .shadow_info = m_shadow_info,
    };
    m_draw_buffer = m_context.create_buffer(sizeof(uint32_t) + m_object_count * sizeof(DrawCmd),
                                            vkb::BufferUsage::IndirectBuffer | vkb::BufferUsage::StorageBuffer |
                                                vkb::BufferUsage::TransferDst,
                                            vk::MemoryUsage::DeviceOnly);
    m_early_draw_buffer_resource->set(m_draw_buffer);
    m_late_draw_buffer_resource->set(m_draw_buffer);

    auto object_buffer =
        m_context.create_buffer(objects.size_bytes(), vkb::BufferUsage::StorageBuffer, vk::MemoryUsage::HostToDevice);
    memcpy(object_buffer.mapped<Object>(), objects.data(), objects.size_bytes());

    m_dynamic_descriptor_buffer = m_context.create_buffer(m_dynamic_set_layout_size,
                                                          vkb::BufferUsage::SamplerDescriptorBufferEXT |
                                                              vkb::BufferUsage::ResourceDescriptorBufferEXT,
                                                          vk::MemoryUsage::HostToDevice);
    vkb::DescriptorAddressInfoEXT uniform_buffer_ai{
        .sType = vkb::StructureType::DescriptorAddressInfoEXT,
        .address = uniform_buffer.device_address(),
        .range = uniform_buffer.size(),
    };
    vkb::DescriptorAddressInfoEXT light_buffer_ai{
        .sType = vkb::StructureType::DescriptorAddressInfoEXT,
        .address = light_buffer.device_address(),
        .range = light_buffer.size(),
    };
    vkb::DescriptorImageInfo output_image_info{
        .imageView = m_output_view,
        .imageLayout = vkb::ImageLayout::General,
    };
    vkb::DescriptorAddressInfoEXT draw_buffer_ai{
        .sType = vkb::StructureType::DescriptorAddressInfoEXT,
        .address = m_draw_buffer.device_address(),
        .range = m_draw_buffer.size(),
    };
    vkb::DescriptorAddressInfoEXT object_buffer_ai{
        .sType = vkb::StructureType::DescriptorAddressInfoEXT,
        .address = object_buffer.device_address(),
        .range = object_buffer.size(),
    };

    auto *descriptor_data = m_dynamic_descriptor_buffer.mapped<uint8_t>();
    descriptor_data = put_descriptor(descriptor_data, vkb::DescriptorType::UniformBuffer, &uniform_buffer_ai);
    descriptor_data = put_descriptor(descriptor_data, vkb::DescriptorType::StorageBuffer, &light_buffer_ai);
    descriptor_data = put_descriptor(descriptor_data, vkb::DescriptorType::StorageImage, &output_image_info);
    descriptor_data = put_descriptor(descriptor_data, vkb::DescriptorType::StorageBuffer, &draw_buffer_ai);
    descriptor_data = put_descriptor(descriptor_data, vkb::DescriptorType::StorageBuffer, &object_buffer_ai);

    // Release ownership to command buffer.
    cmd_buf.bind_associated_buffer(vull::move(uniform_buffer));
    cmd_buf.bind_associated_buffer(vull::move(light_buffer));
    cmd_buf.bind_associated_buffer(vull::move(object_buffer));
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

void DefaultRenderer::compile_render_graph() {
    m_render_graph.compile(*m_output_image_resource);
}

void DefaultRenderer::load_scene(Scene &scene, vpak::Reader &pack_reader) {
    m_scene = &scene;
    m_texture_descriptor_buffer = m_context.create_buffer(
        scene.texture_count() * m_context.descriptor_size(vkb::DescriptorType::CombinedImageSampler),
        vkb::BufferUsage::SamplerDescriptorBufferEXT | vkb::BufferUsage::TransferDst, vk::MemoryUsage::DeviceOnly);

    auto texture_descriptor_staging_buffer = m_texture_descriptor_buffer.create_staging();
    auto *descriptor_data = texture_descriptor_staging_buffer.mapped<uint8_t>();
    for (uint32_t i = 0; i < scene.texture_count(); i++) {
        vkb::DescriptorImageInfo image_info{
            .sampler = scene.texture_samplers()[i],
            .imageView = *scene.texture_images()[i].full_view(),
            .imageLayout = vkb::ImageLayout::ReadOnlyOptimal,
        };
        descriptor_data = put_descriptor(descriptor_data, vkb::DescriptorType::CombinedImageSampler, &image_info);
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

void DefaultRenderer::render(vk::CommandBuffer &cmd_buf, const Mat4f &proj, const Mat4f &view,
                             const Vec3f &view_position, vkb::Image output_image, vkb::ImageView output_view,
                             Optional<const vk::QueryPool &> timestamp_pool) {
    m_proj = proj;
    m_view = view;
    m_view_position = view_position;
    m_output_view = output_view;
    m_output_image_resource->set(output_image, output_view);
    update_cascades();

    vkb::MemoryBarrier2 memory_barrier{
        .sType = vkb::StructureType::MemoryBarrier2,
        .srcStageMask = vkb::PipelineStage2::ColorAttachmentOutput,
        .srcAccessMask = vkb::Access2::ColorAttachmentWrite,
        .dstStageMask = vkb::PipelineStage2::AllCommands,
        .dstAccessMask = vkb::Access2::MemoryRead,
    };
    cmd_buf.pipeline_barrier(vkb::DependencyInfo{
        .sType = vkb::StructureType::DependencyInfo,
        .memoryBarrierCount = 1,
        .pMemoryBarriers = &memory_barrier,
    });
    update_buffers(cmd_buf);
    m_render_graph.record(cmd_buf, timestamp_pool);
    cmd_buf.bind_associated_buffer(vull::move(m_dynamic_descriptor_buffer));
    cmd_buf.bind_associated_buffer(vull::move(m_draw_buffer));
}

} // namespace vull
