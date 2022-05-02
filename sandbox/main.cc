#include "Camera.hh"
#include "SceneLoader.hh"

#include <vull/core/Material.hh>
#include <vull/core/Mesh.hh>
#include <vull/core/Transform.hh>
#include <vull/core/Vertex.hh>
#include <vull/core/Window.hh>
#include <vull/ecs/Entity.hh>
#include <vull/ecs/EntityId.hh>
#include <vull/ecs/World.hh>
#include <vull/maths/Common.hh>
#include <vull/maths/Mat.hh>
#include <vull/maths/Vec.hh>
#include <vull/support/Array.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Format.hh>
#include <vull/support/String.hh>
#include <vull/support/Timer.hh>
#include <vull/support/Tuple.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>
#include <vull/tasklet/Scheduler.hh>
#include <vull/tasklet/Tasklet.hh> // IWYU pragma: keep
#include <vull/ui/Renderer.hh>
#include <vull/ui/TimeGraph.hh>
#include <vull/vpak/PackReader.hh>
#include <vull/vulkan/CommandBuffer.hh>
#include <vull/vulkan/CommandPool.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/Queue.hh>
#include <vull/vulkan/Swapchain.hh>
#include <vull/vulkan/Vulkan.hh>

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace vull;

namespace {

uint32_t find_graphics_family(const VkContext &context) {
    for (uint32_t i = 0; i < context.queue_families().size(); i++) {
        const auto &family = context.queue_families()[i];
        if ((family.queueFlags & vk::QueueFlags::Graphics) != vk::QueueFlags::None) {
            return i;
        }
    }
    VULL_ENSURE_NOT_REACHED();
}

Mat4f get_transform_matrix(World &world, EntityId id) {
    const auto &transform = world.get_component<Transform>(id);
    if (transform.parent() == id) {
        // Root node.
        return {1.0f};
    }
    const auto parent_matrix = get_transform_matrix(world, transform.parent());
    return parent_matrix * transform.matrix();
}

vk::ShaderModule load_shader(const VkContext &context, const char *path) {
    FILE *file = fopen(path, "rb");
    fseek(file, 0, SEEK_END);
    LargeVector<uint32_t> binary(static_cast<size_t>(ftell(file)) / sizeof(uint32_t));
    fseek(file, 0, SEEK_SET);
    VULL_ENSURE(fread(binary.data(), sizeof(uint32_t), binary.size(), file) == binary.size());
    fclose(file);
    vk::ShaderModuleCreateInfo module_ci{
        .sType = vk::StructureType::ShaderModuleCreateInfo,
        .codeSize = binary.size_bytes(),
        .pCode = binary.data(),
    };
    vk::ShaderModule module;
    VULL_ENSURE(context.vkCreateShaderModule(&module_ci, &module) == vk::Result::Success);
    return module;
}

void main_task(Scheduler &scheduler) {
    Window window(2560, 1440, false);
    VkContext context;
    auto swapchain = window.create_swapchain(context);

    const auto graphics_family_index = find_graphics_family(context);
    CommandPool command_pool(context, graphics_family_index);
    Queue queue(context, graphics_family_index);

    vk::MemoryRequirements scene_memory_requirements{
        .size = 1024ul * 1024ul * 2048ul,
        .memoryTypeBits = 0xffffffffu,
    };
    auto *scene_memory = context.allocate_memory(scene_memory_requirements, MemoryType::DeviceLocal);

    auto *pack_file = fopen("scene.vpak", "rb");
    PackReader pack_reader(pack_file);
    World world;
    Vector<vk::Buffer> vertex_buffers;
    Vector<vk::Buffer> index_buffers;
    Vector<vk::Image> texture_images;
    Vector<vk::ImageView> texture_image_views;
    load_scene(context, pack_reader, command_pool, queue, world, vertex_buffers, index_buffers, texture_images,
               texture_image_views, scene_memory);
    fclose(pack_file);

    constexpr uint32_t tile_size = 32;
    uint32_t row_tile_count = (window.width() + (window.width() % tile_size)) / tile_size;
    uint32_t col_tile_count = (window.height() + (window.height() % tile_size)) / tile_size;

    struct SpecialisationData {
        uint32_t tile_size;
        uint32_t tile_max_light_count;
        uint32_t row_tile_count;
        uint32_t viewport_width;
        uint32_t viewport_height;
    } specialisation_data{
        .tile_size = tile_size,
        .tile_max_light_count = 400,
        .row_tile_count = row_tile_count,
        .viewport_width = window.width(),
        .viewport_height = window.height(),
    };

    Array specialisation_map_entries{
        vk::SpecializationMapEntry{
            .constantID = 0,
            .offset = offsetof(SpecialisationData, tile_size),
            .size = sizeof(SpecialisationData::tile_size),
        },
        vk::SpecializationMapEntry{
            .constantID = 1,
            .offset = offsetof(SpecialisationData, tile_max_light_count),
            .size = sizeof(SpecialisationData::tile_max_light_count),
        },
        vk::SpecializationMapEntry{
            .constantID = 2,
            .offset = offsetof(SpecialisationData, row_tile_count),
            .size = sizeof(SpecialisationData::row_tile_count),
        },
        vk::SpecializationMapEntry{
            .constantID = 3,
            .offset = offsetof(SpecialisationData, viewport_width),
            .size = sizeof(SpecialisationData::viewport_width),
        },
        vk::SpecializationMapEntry{
            .constantID = 4,
            .offset = offsetof(SpecialisationData, viewport_height),
            .size = sizeof(SpecialisationData::viewport_height),
        },
    };
    vk::SpecializationInfo specialisation_info{
        .mapEntryCount = specialisation_map_entries.size(),
        .pMapEntries = specialisation_map_entries.data(),
        .dataSize = sizeof(SpecialisationData),
        .pData = &specialisation_data,
    };

    auto *light_cull_shader = load_shader(context, "engine/shaders/light_cull.comp.spv");
    auto *main_vertex_shader = load_shader(context, "engine/shaders/main.vert.spv");
    auto *main_fragment_shader = load_shader(context, "engine/shaders/main.frag.spv");
    auto *shadow_shader = load_shader(context, "engine/shaders/shadow.vert.spv");
    auto *ui_vertex_shader = load_shader(context, "engine/shaders/ui.vert.spv");
    auto *ui_fragment_shader = load_shader(context, "engine/shaders/ui.frag.spv");
    vk::PipelineShaderStageCreateInfo depth_pass_shader_stage_ci{
        .sType = vk::StructureType::PipelineShaderStageCreateInfo,
        .stage = vk::ShaderStage::Vertex,
        .module = main_vertex_shader,
        .pName = "main",
    };
    vk::PipelineShaderStageCreateInfo light_cull_shader_stage_ci{
        .sType = vk::StructureType::PipelineShaderStageCreateInfo,
        .stage = vk::ShaderStage::Compute,
        .module = light_cull_shader,
        .pName = "main",
        .pSpecializationInfo = &specialisation_info,
    };
    Array main_shader_stage_cis{
        vk::PipelineShaderStageCreateInfo{
            .sType = vk::StructureType::PipelineShaderStageCreateInfo,
            .stage = vk::ShaderStage::Vertex,
            .module = main_vertex_shader,
            .pName = "main",
        },
        vk::PipelineShaderStageCreateInfo{
            .sType = vk::StructureType::PipelineShaderStageCreateInfo,
            .stage = vk::ShaderStage::Fragment,
            .module = main_fragment_shader,
            .pName = "main",
            .pSpecializationInfo = &specialisation_info,
        },
    };
    vk::PipelineShaderStageCreateInfo shadow_shader_stage_ci{
        .sType = vk::StructureType::PipelineShaderStageCreateInfo,
        .stage = vk::ShaderStage::Vertex,
        .module = shadow_shader,
        .pName = "main",
        .pSpecializationInfo = &specialisation_info,
    };

    Array set_bindings{
        vk::DescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = vk::DescriptorType::UniformBuffer,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStage::All,
        },
        vk::DescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = vk::DescriptorType::StorageBuffer,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStage::Compute | vk::ShaderStage::Fragment,
        },
        vk::DescriptorSetLayoutBinding{
            .binding = 2,
            .descriptorType = vk::DescriptorType::StorageBuffer,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStage::Compute | vk::ShaderStage::Fragment,
        },
        vk::DescriptorSetLayoutBinding{
            .binding = 3,
            .descriptorType = vk::DescriptorType::CombinedImageSampler,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStage::Compute,
        },
        vk::DescriptorSetLayoutBinding{
            .binding = 4,
            .descriptorType = vk::DescriptorType::CombinedImageSampler,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStage::Fragment,
        },
        vk::DescriptorSetLayoutBinding{
            .binding = 5,
            .descriptorType = vk::DescriptorType::Sampler,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStage::Fragment,
        },
        vk::DescriptorSetLayoutBinding{
            .binding = 6,
            .descriptorType = vk::DescriptorType::Sampler,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStage::Fragment,
        },
        vk::DescriptorSetLayoutBinding{
            .binding = 7,
            .descriptorType = vk::DescriptorType::SampledImage,
            .descriptorCount = texture_image_views.size(),
            .stageFlags = vk::ShaderStage::Fragment,
        },
    };
    vk::DescriptorSetLayoutCreateInfo set_layout_ci{
        .sType = vk::StructureType::DescriptorSetLayoutCreateInfo,
        .bindingCount = set_bindings.size(),
        .pBindings = set_bindings.data(),
    };
    vk::DescriptorSetLayout set_layout;
    VULL_ENSURE(context.vkCreateDescriptorSetLayout(&set_layout_ci, &set_layout) == vk::Result::Success);

    struct PushConstantBlock {
        Mat4f transform;
        uint32_t albedo_index;
        uint32_t normal_index;
        uint32_t cascade_index;
    };
    vk::PushConstantRange push_constant_range{
        .stageFlags = vk::ShaderStage::Vertex | vk::ShaderStage::Fragment,
        .size = sizeof(PushConstantBlock),
    };
    vk::PipelineLayoutCreateInfo pipeline_layout_ci{
        .sType = vk::StructureType::PipelineLayoutCreateInfo,
        .setLayoutCount = 1,
        .pSetLayouts = &set_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_constant_range,
    };
    vk::PipelineLayout pipeline_layout;
    VULL_ENSURE(context.vkCreatePipelineLayout(&pipeline_layout_ci, &pipeline_layout) == vk::Result::Success);

    Array vertex_attribute_descriptions{
        vk::VertexInputAttributeDescription{
            .location = 0,
            .format = vk::Format::R32G32B32Sfloat,
            .offset = offsetof(Vertex, position),
        },
        vk::VertexInputAttributeDescription{
            .location = 1,
            .format = vk::Format::R32G32B32Sfloat,
            .offset = offsetof(Vertex, normal),
        },
        vk::VertexInputAttributeDescription{
            .location = 2,
            .format = vk::Format::R32G32Sfloat,
            .offset = offsetof(Vertex, uv),
        },
    };
    vk::VertexInputBindingDescription vertex_binding_description{
        .stride = sizeof(Vertex),
        .inputRate = vk::VertexInputRate::Vertex,
    };
    vk::PipelineVertexInputStateCreateInfo vertex_input_state{
        .sType = vk::StructureType::PipelineVertexInputStateCreateInfo,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &vertex_binding_description,
        .vertexAttributeDescriptionCount = vertex_attribute_descriptions.size(),
        .pVertexAttributeDescriptions = vertex_attribute_descriptions.data(),
    };
    vk::PipelineInputAssemblyStateCreateInfo input_assembly_state{
        .sType = vk::StructureType::PipelineInputAssemblyStateCreateInfo,
        .topology = vk::PrimitiveTopology::TriangleList,
    };

    vk::Rect2D scissor{
        .extent = swapchain.extent_2D(),
    };
    vk::Viewport viewport{
        .width = static_cast<float>(window.width()),
        .height = static_cast<float>(window.height()),
        .maxDepth = 1.0f,
    };
    vk::PipelineViewportStateCreateInfo viewport_state{
        .sType = vk::StructureType::PipelineViewportStateCreateInfo,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor,
    };

    constexpr uint32_t shadow_resolution = 2048;
    vk::Rect2D shadow_scissor{
        .extent = {shadow_resolution, shadow_resolution},
    };
    vk::Viewport shadow_viewport{
        .width = shadow_resolution,
        .height = shadow_resolution,
        .maxDepth = 1.0f,
    };
    vk::PipelineViewportStateCreateInfo shadow_pass_viewport_state{
        .sType = vk::StructureType::PipelineViewportStateCreateInfo,
        .viewportCount = 1,
        .pViewports = &shadow_viewport,
        .scissorCount = 1,
        .pScissors = &shadow_scissor,
    };

    vk::PipelineRasterizationStateCreateInfo rasterisation_state{
        .sType = vk::StructureType::PipelineRasterizationStateCreateInfo,
        .polygonMode = vk::PolygonMode::Fill,
        .cullMode = vk::CullMode::Back,
        .frontFace = vk::FrontFace::CounterClockwise,
        .lineWidth = 1.0f,
    };
    vk::PipelineRasterizationStateCreateInfo shadow_pass_rasterisation_state{
        .sType = vk::StructureType::PipelineRasterizationStateCreateInfo,
        .polygonMode = vk::PolygonMode::Fill,
        .cullMode = vk::CullMode::Back,
        .frontFace = vk::FrontFace::CounterClockwise,
        .depthBiasEnable = true,
        .depthBiasConstantFactor = 1.25f,
        .depthBiasSlopeFactor = 1.75f,
        .lineWidth = 1.0f,
    };

    vk::PipelineMultisampleStateCreateInfo multisample_state{
        .sType = vk::StructureType::PipelineMultisampleStateCreateInfo,
        .rasterizationSamples = vk::SampleCount::_1,
        .minSampleShading = 1.0f,
    };

    vk::PipelineDepthStencilStateCreateInfo depth_pass_depth_stencil_state{
        .sType = vk::StructureType::PipelineDepthStencilStateCreateInfo,
        .depthTestEnable = true,
        .depthWriteEnable = true,
        .depthCompareOp = vk::CompareOp::GreaterOrEqual,
    };
    vk::PipelineDepthStencilStateCreateInfo shadow_pass_depth_stencil_state{
        .sType = vk::StructureType::PipelineDepthStencilStateCreateInfo,
        .depthTestEnable = true,
        .depthWriteEnable = true,
        .depthCompareOp = vk::CompareOp::LessOrEqual,
    };
    vk::PipelineDepthStencilStateCreateInfo main_pass_depth_stencil_state{
        .sType = vk::StructureType::PipelineDepthStencilStateCreateInfo,
        .depthTestEnable = true,
        .depthCompareOp = vk::CompareOp::Equal,
    };

    vk::PipelineColorBlendAttachmentState main_pass_blend_attachment{
        .colorWriteMask = vk::ColorComponent::R | vk::ColorComponent::G | vk::ColorComponent::B | vk::ColorComponent::A,
    };
    vk::PipelineColorBlendStateCreateInfo main_pass_blend_state{
        .sType = vk::StructureType::PipelineColorBlendStateCreateInfo,
        .attachmentCount = 1,
        .pAttachments = &main_pass_blend_attachment,
    };

    const auto depth_format = vk::Format::D32Sfloat;
    vk::PipelineRenderingCreateInfo depth_pass_rendering_create_info{
        .sType = vk::StructureType::PipelineRenderingCreateInfo,
        .depthAttachmentFormat = depth_format,
        .stencilAttachmentFormat = depth_format,
    };
    vk::GraphicsPipelineCreateInfo depth_pass_pipeline_ci{
        .sType = vk::StructureType::GraphicsPipelineCreateInfo,
        .pNext = &depth_pass_rendering_create_info,
        .stageCount = 1,
        .pStages = &depth_pass_shader_stage_ci,
        .pVertexInputState = &vertex_input_state,
        .pInputAssemblyState = &input_assembly_state,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterisation_state,
        .pMultisampleState = &multisample_state,
        .pDepthStencilState = &depth_pass_depth_stencil_state,
        .layout = pipeline_layout,
    };
    vk::Pipeline depth_pass_pipeline;
    VULL_ENSURE(context.vkCreateGraphicsPipelines(nullptr, 1, &depth_pass_pipeline_ci, &depth_pass_pipeline) ==
                vk::Result::Success);

    vk::ComputePipelineCreateInfo light_cull_pipeline_ci{
        .sType = vk::StructureType::ComputePipelineCreateInfo,
        .stage = light_cull_shader_stage_ci,
        .layout = pipeline_layout,
    };
    vk::Pipeline light_cull_pipeline;
    VULL_ENSURE(context.vkCreateComputePipelines(nullptr, 1, &light_cull_pipeline_ci, &light_cull_pipeline) ==
                vk::Result::Success);

    vk::PipelineRenderingCreateInfo shadow_pass_rendering_create_info{
        .sType = vk::StructureType::PipelineRenderingCreateInfo,
        .depthAttachmentFormat = vk::Format::D32Sfloat,
        .stencilAttachmentFormat = vk::Format::D32Sfloat,
    };
    vk::GraphicsPipelineCreateInfo shadow_pass_pipeline_ci{
        .sType = vk::StructureType::GraphicsPipelineCreateInfo,
        .pNext = &shadow_pass_rendering_create_info,
        .stageCount = 1,
        .pStages = &shadow_shader_stage_ci,
        .pVertexInputState = &vertex_input_state,
        .pInputAssemblyState = &input_assembly_state,
        .pViewportState = &shadow_pass_viewport_state,
        .pRasterizationState = &shadow_pass_rasterisation_state,
        .pMultisampleState = &multisample_state,
        .pDepthStencilState = &shadow_pass_depth_stencil_state,
        .layout = pipeline_layout,
    };
    vk::Pipeline shadow_pass_pipeline;
    VULL_ENSURE(context.vkCreateGraphicsPipelines(nullptr, 1, &shadow_pass_pipeline_ci, &shadow_pass_pipeline) ==
                vk::Result::Success);

    const auto colour_format = vk::Format::B8G8R8A8Srgb;
    vk::PipelineRenderingCreateInfo main_pass_rendering_create_info{
        .sType = vk::StructureType::PipelineRenderingCreateInfo,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &colour_format,
        .depthAttachmentFormat = depth_format,
        .stencilAttachmentFormat = depth_format,
    };
    vk::GraphicsPipelineCreateInfo main_pass_pipeline_ci{
        .sType = vk::StructureType::GraphicsPipelineCreateInfo,
        .pNext = &main_pass_rendering_create_info,
        .stageCount = main_shader_stage_cis.size(),
        .pStages = main_shader_stage_cis.data(),
        .pVertexInputState = &vertex_input_state,
        .pInputAssemblyState = &input_assembly_state,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterisation_state,
        .pMultisampleState = &multisample_state,
        .pDepthStencilState = &main_pass_depth_stencil_state,
        .pColorBlendState = &main_pass_blend_state,
        .layout = pipeline_layout,
    };
    vk::Pipeline main_pass_pipeline;
    VULL_ENSURE(context.vkCreateGraphicsPipelines(nullptr, 1, &main_pass_pipeline_ci, &main_pass_pipeline) ==
                vk::Result::Success);

    vk::ImageCreateInfo depth_image_ci{
        .sType = vk::StructureType::ImageCreateInfo,
        .imageType = vk::ImageType::_2D,
        .format = depth_format,
        .extent = swapchain.extent_3D(),
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = vk::SampleCount::_1,
        .tiling = vk::ImageTiling::Optimal,
        .usage = vk::ImageUsage::DepthStencilAttachment | vk::ImageUsage::Sampled,
        .sharingMode = vk::SharingMode::Exclusive,
        .initialLayout = vk::ImageLayout::Undefined,
    };
    vk::Image depth_image;
    VULL_ENSURE(context.vkCreateImage(&depth_image_ci, &depth_image) == vk::Result::Success);

    vk::MemoryRequirements depth_image_requirements{};
    context.vkGetImageMemoryRequirements(depth_image, &depth_image_requirements);
    vk::DeviceMemory depth_image_memory = context.allocate_memory(depth_image_requirements, MemoryType::DeviceLocal);
    VULL_ENSURE(context.vkBindImageMemory(depth_image, depth_image_memory, 0) == vk::Result::Success);

    vk::ImageViewCreateInfo depth_image_view_ci{
        .sType = vk::StructureType::ImageViewCreateInfo,
        .image = depth_image,
        .viewType = vk::ImageViewType::_2D,
        .format = depth_format,
        .subresourceRange{
            .aspectMask = vk::ImageAspect::Depth,
            .levelCount = 1,
            .layerCount = 1,
        },
    };
    vk::ImageView depth_image_view;
    VULL_ENSURE(context.vkCreateImageView(&depth_image_view_ci, &depth_image_view) == vk::Result::Success);

    constexpr uint32_t shadow_cascade_count = 4;
    vk::ImageCreateInfo shadow_map_ci{
        .sType = vk::StructureType::ImageCreateInfo,
        .imageType = vk::ImageType::_2D,
        .format = vk::Format::D32Sfloat,
        .extent = {shadow_resolution, shadow_resolution, 1},
        .mipLevels = 1,
        .arrayLayers = shadow_cascade_count,
        .samples = vk::SampleCount::_1,
        .tiling = vk::ImageTiling::Optimal,
        .usage = vk::ImageUsage::DepthStencilAttachment | vk::ImageUsage::Sampled,
        .sharingMode = vk::SharingMode::Exclusive,
        .initialLayout = vk::ImageLayout::Undefined,
    };
    vk::Image shadow_map;
    VULL_ENSURE(context.vkCreateImage(&shadow_map_ci, &shadow_map) == vk::Result::Success);

    vk::MemoryRequirements shadow_map_requirements{};
    context.vkGetImageMemoryRequirements(shadow_map, &shadow_map_requirements);
    vk::DeviceMemory shadow_map_memory = context.allocate_memory(shadow_map_requirements, MemoryType::DeviceLocal);
    VULL_ENSURE(context.vkBindImageMemory(shadow_map, shadow_map_memory, 0) == vk::Result::Success);

    vk::ImageViewCreateInfo shadow_map_view_ci{
        .sType = vk::StructureType::ImageViewCreateInfo,
        .image = shadow_map,
        .viewType = vk::ImageViewType::_2DArray,
        .format = shadow_map_ci.format,
        .subresourceRange{
            .aspectMask = vk::ImageAspect::Depth,
            .levelCount = 1,
            .layerCount = shadow_cascade_count,
        },
    };
    vk::ImageView shadow_map_view;
    VULL_ENSURE(context.vkCreateImageView(&shadow_map_view_ci, &shadow_map_view) == vk::Result::Success);

    Vector<vk::ImageView> shadow_cascade_views(shadow_cascade_count);
    for (uint32_t i = 0; i < shadow_cascade_count; i++) {
        vk::ImageViewCreateInfo view_ci{
            .sType = vk::StructureType::ImageViewCreateInfo,
            .image = shadow_map,
            .viewType = vk::ImageViewType::_2DArray,
            .format = shadow_map_ci.format,
            .subresourceRange{
                .aspectMask = vk::ImageAspect::Depth,
                .levelCount = 1,
                .baseArrayLayer = i,
                .layerCount = 1,
            },
        };
        VULL_ENSURE(context.vkCreateImageView(&view_ci, &shadow_cascade_views[i]) == vk::Result::Success);
    }

    vk::SamplerCreateInfo depth_sampler_ci{
        .sType = vk::StructureType::SamplerCreateInfo,
        .magFilter = vk::Filter::Nearest,
        .minFilter = vk::Filter::Nearest,
        .mipmapMode = vk::SamplerMipmapMode::Nearest,
        .addressModeU = vk::SamplerAddressMode::ClampToEdge,
        .addressModeV = vk::SamplerAddressMode::ClampToEdge,
        .addressModeW = vk::SamplerAddressMode::ClampToEdge,
        .borderColor = vk::BorderColor::FloatOpaqueWhite,
    };
    vk::Sampler depth_sampler;
    VULL_ENSURE(context.vkCreateSampler(&depth_sampler_ci, &depth_sampler) == vk::Result::Success);

    vk::SamplerCreateInfo albedo_sampler_ci{
        .sType = vk::StructureType::SamplerCreateInfo,
        // TODO: Switch back to linear filtering; create a separate sampler for things wanting nearest filtering (error
        //       texture).
        .magFilter = vk::Filter::Nearest,
        .minFilter = vk::Filter::Nearest,
        .mipmapMode = vk::SamplerMipmapMode::Linear,
        .addressModeU = vk::SamplerAddressMode::Repeat,
        .addressModeV = vk::SamplerAddressMode::Repeat,
        .addressModeW = vk::SamplerAddressMode::Repeat,
        .anisotropyEnable = true,
        .maxAnisotropy = 16.0f,
        // TODO: Bistro's mipmap levels smaller than 16x16 seem to be really broken.
        .maxLod = 7.0f,
        .borderColor = vk::BorderColor::FloatTransparentBlack,
    };
    vk::Sampler albedo_sampler;
    VULL_ENSURE(context.vkCreateSampler(&albedo_sampler_ci, &albedo_sampler) == vk::Result::Success);

    vk::SamplerCreateInfo normal_sampler_ci{
        .sType = vk::StructureType::SamplerCreateInfo,
        .magFilter = vk::Filter::Linear,
        .minFilter = vk::Filter::Linear,
        .mipmapMode = vk::SamplerMipmapMode::Linear,
        .addressModeU = vk::SamplerAddressMode::Repeat,
        .addressModeV = vk::SamplerAddressMode::Repeat,
        .addressModeW = vk::SamplerAddressMode::Repeat,
        .anisotropyEnable = true,
        .maxAnisotropy = 16.0f,
        .maxLod = vk::k_lod_clamp_none,
        .borderColor = vk::BorderColor::FloatTransparentBlack,
    };
    vk::Sampler normal_sampler;
    VULL_ENSURE(context.vkCreateSampler(&normal_sampler_ci, &normal_sampler) == vk::Result::Success);

    struct UniformBuffer {
        Mat4f proj;
        Mat4f view;
        Array<Mat4f, 4> sun_matrices;
        Vec3f camera_position;
        Vec4f sun_cascade_split_depths;
    };
    vk::BufferCreateInfo uniform_buffer_ci{
        .sType = vk::StructureType::BufferCreateInfo,
        .size = sizeof(UniformBuffer),
        .usage = vk::BufferUsage::UniformBuffer,
        .sharingMode = vk::SharingMode::Exclusive,
    };
    vk::Buffer uniform_buffer;
    VULL_ENSURE(context.vkCreateBuffer(&uniform_buffer_ci, &uniform_buffer) == vk::Result::Success);

    vk::MemoryRequirements uniform_buffer_requirements{};
    context.vkGetBufferMemoryRequirements(uniform_buffer, &uniform_buffer_requirements);
    vk::DeviceMemory uniform_buffer_memory =
        context.allocate_memory(uniform_buffer_requirements, MemoryType::HostVisible);
    VULL_ENSURE(context.vkBindBufferMemory(uniform_buffer, uniform_buffer_memory, 0) == vk::Result::Success);

    struct PointLight {
        Vec3f position;
        float radius{0.0f};
        Vec3f colour;
        float padding{0.0f};
    };
    vk::DeviceSize lights_buffer_size = sizeof(PointLight) * 3000 + sizeof(float) * 4;
    vk::DeviceSize light_visibility_size = (specialisation_data.tile_max_light_count + 1) * sizeof(uint32_t);
    vk::DeviceSize light_visibilities_buffer_size = light_visibility_size * row_tile_count * col_tile_count;

    vk::BufferCreateInfo lights_buffer_ci{
        .sType = vk::StructureType::BufferCreateInfo,
        .size = lights_buffer_size,
        .usage = vk::BufferUsage::StorageBuffer,
        .sharingMode = vk::SharingMode::Exclusive,
    };
    vk::Buffer lights_buffer;
    VULL_ENSURE(context.vkCreateBuffer(&lights_buffer_ci, &lights_buffer) == vk::Result::Success);

    vk::MemoryRequirements lights_buffer_requirements{};
    context.vkGetBufferMemoryRequirements(lights_buffer, &lights_buffer_requirements);
    vk::DeviceMemory lights_buffer_memory =
        context.allocate_memory(lights_buffer_requirements, MemoryType::HostVisible);
    VULL_ENSURE(context.vkBindBufferMemory(lights_buffer, lights_buffer_memory, 0) == vk::Result::Success);

    vk::BufferCreateInfo light_visibilities_buffer_ci{
        .sType = vk::StructureType::BufferCreateInfo,
        .size = light_visibilities_buffer_size,
        .usage = vk::BufferUsage::StorageBuffer,
        .sharingMode = vk::SharingMode::Exclusive,
    };
    vk::Buffer light_visibilities_buffer;
    VULL_ENSURE(context.vkCreateBuffer(&light_visibilities_buffer_ci, &light_visibilities_buffer) ==
                vk::Result::Success);

    vk::MemoryRequirements light_visibilities_buffer_requirements{};
    context.vkGetBufferMemoryRequirements(light_visibilities_buffer, &light_visibilities_buffer_requirements);
    vk::DeviceMemory light_visibilities_buffer_memory =
        context.allocate_memory(light_visibilities_buffer_requirements, MemoryType::DeviceLocal);
    VULL_ENSURE(context.vkBindBufferMemory(light_visibilities_buffer, light_visibilities_buffer_memory, 0) ==
                vk::Result::Success);

    Array descriptor_pool_sizes{
        vk::DescriptorPoolSize{
            .type = vk::DescriptorType::Sampler,
            .descriptorCount = 2,
        },
        vk::DescriptorPoolSize{
            .type = vk::DescriptorType::SampledImage,
            .descriptorCount = texture_image_views.size(),
        },
        vk::DescriptorPoolSize{
            .type = vk::DescriptorType::UniformBuffer,
            .descriptorCount = 1,
        },
        vk::DescriptorPoolSize{
            .type = vk::DescriptorType::StorageBuffer,
            .descriptorCount = 2,
        },
        vk::DescriptorPoolSize{
            .type = vk::DescriptorType::CombinedImageSampler,
            .descriptorCount = 2,
        },
    };
    vk::DescriptorPoolCreateInfo descriptor_pool_ci{
        .sType = vk::StructureType::DescriptorPoolCreateInfo,
        .maxSets = 1,
        .poolSizeCount = descriptor_pool_sizes.size(),
        .pPoolSizes = descriptor_pool_sizes.data(),
    };
    vk::DescriptorPool descriptor_pool;
    VULL_ENSURE(context.vkCreateDescriptorPool(&descriptor_pool_ci, &descriptor_pool) == vk::Result::Success);

    vk::DescriptorSetAllocateInfo descriptor_set_ai{
        .sType = vk::StructureType::DescriptorSetAllocateInfo,
        .descriptorPool = descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &set_layout,
    };
    vk::DescriptorSet descriptor_set;
    VULL_ENSURE(context.vkAllocateDescriptorSets(&descriptor_set_ai, &descriptor_set) == vk::Result::Success);

    vk::DescriptorBufferInfo uniform_buffer_info{
        .buffer = uniform_buffer,
        .range = vk::k_whole_size,
    };
    vk::DescriptorBufferInfo lights_buffer_info{
        .buffer = lights_buffer,
        .range = vk::k_whole_size,
    };
    vk::DescriptorBufferInfo light_visibilities_buffer_info{
        .buffer = light_visibilities_buffer,
        .range = vk::k_whole_size,
    };
    vk::DescriptorImageInfo depth_sampler_image_info{
        .sampler = depth_sampler,
        .imageView = depth_image_view,
        .imageLayout = vk::ImageLayout::ShaderReadOnlyOptimal,
    };
    vk::DescriptorImageInfo shadow_map_image_info{
        .sampler = depth_sampler,
        .imageView = shadow_map_view,
        .imageLayout = vk::ImageLayout::ShaderReadOnlyOptimal,
    };
    vk::DescriptorImageInfo albedo_sampler_info{
        .sampler = albedo_sampler,
    };
    vk::DescriptorImageInfo normal_sampler_info{
        .sampler = normal_sampler,
    };
    Vector<vk::DescriptorImageInfo> texture_image_infos;
    texture_image_infos.ensure_capacity(texture_image_views.size());
    for (auto *image_view : texture_image_views) {
        texture_image_infos.push(vk::DescriptorImageInfo{
            .imageView = image_view,
            .imageLayout = vk::ImageLayout::ShaderReadOnlyOptimal,
        });
    }
    Array descriptor_writes{
        vk::WriteDescriptorSet{
            .sType = vk::StructureType::WriteDescriptorSet,
            .dstSet = descriptor_set,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::UniformBuffer,
            .pBufferInfo = &uniform_buffer_info,
        },
        vk::WriteDescriptorSet{
            .sType = vk::StructureType::WriteDescriptorSet,
            .dstSet = descriptor_set,
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::StorageBuffer,
            .pBufferInfo = &lights_buffer_info,
        },
        vk::WriteDescriptorSet{
            .sType = vk::StructureType::WriteDescriptorSet,
            .dstSet = descriptor_set,
            .dstBinding = 2,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::StorageBuffer,
            .pBufferInfo = &light_visibilities_buffer_info,
        },
        vk::WriteDescriptorSet{
            .sType = vk::StructureType::WriteDescriptorSet,
            .dstSet = descriptor_set,
            .dstBinding = 3,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::CombinedImageSampler,
            .pImageInfo = &depth_sampler_image_info,
        },
        vk::WriteDescriptorSet{
            .sType = vk::StructureType::WriteDescriptorSet,
            .dstSet = descriptor_set,
            .dstBinding = 4,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::CombinedImageSampler,
            .pImageInfo = &shadow_map_image_info,
        },
        vk::WriteDescriptorSet{
            .sType = vk::StructureType::WriteDescriptorSet,
            .dstSet = descriptor_set,
            .dstBinding = 5,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::Sampler,
            .pImageInfo = &albedo_sampler_info,
        },
        vk::WriteDescriptorSet{
            .sType = vk::StructureType::WriteDescriptorSet,
            .dstSet = descriptor_set,
            .dstBinding = 6,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::Sampler,
            .pImageInfo = &normal_sampler_info,
        },
        vk::WriteDescriptorSet{
            .sType = vk::StructureType::WriteDescriptorSet,
            .dstSet = descriptor_set,
            .dstBinding = 7,
            .descriptorCount = texture_image_infos.size(),
            .descriptorType = vk::DescriptorType::SampledImage,
            .pImageInfo = texture_image_infos.data(),
        },
    };
    context.vkUpdateDescriptorSets(descriptor_writes.size(), descriptor_writes.data(), 0, nullptr);

    vk::FenceCreateInfo fence_ci{
        .sType = vk::StructureType::FenceCreateInfo,
        .flags = vk::FenceCreateFlags::Signaled,
    };
    vk::Fence fence;
    VULL_ENSURE(context.vkCreateFence(&fence_ci, &fence) == vk::Result::Success);

    vk::SemaphoreCreateInfo semaphore_ci{
        .sType = vk::StructureType::SemaphoreCreateInfo,
    };
    vk::Semaphore image_available_semaphore;
    vk::Semaphore rendering_finished_semaphore;
    VULL_ENSURE(context.vkCreateSemaphore(&semaphore_ci, &image_available_semaphore) == vk::Result::Success);
    VULL_ENSURE(context.vkCreateSemaphore(&semaphore_ci, &rendering_finished_semaphore) == vk::Result::Success);

    srand(0);
    auto rand_float = [](float min, float max) {
        return min + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX) / (max - min));
    };

    Vector<PointLight> lights(50);
    for (auto &light : lights) {
        light.colour = {rand_float(0.1f, 1.0f), rand_float(0.1f, 1.0f), rand_float(0.1f, 1.0f)};
        light.radius = rand_float(2.5f, 15.0f);
        light.position[0] = rand_float(-50.0f, 100.0f);
        light.position[1] = rand_float(2.0f, 30.0f);
        light.position[2] = rand_float(-70.0f, 50.0f);
    }

    Camera camera;
    camera.set_position({20.0f, 15.0f, -20.0f});
    camera.set_pitch(-0.3f);
    camera.set_yaw(2.4f);

    const float near_plane = 0.1f;
    UniformBuffer ubo{
        .proj = vull::infinite_perspective(window.aspect_ratio(), vull::half_pi<float>, near_plane),
    };

    auto update_cascades = [&] {
        const float shadow_distance = 500.0f;
        const float clip_range = shadow_distance - near_plane;
        const float split_lambda = 0.95f;
        Array<float, 4> split_distances;
        for (uint32_t i = 0; i < shadow_cascade_count; i++) {
            float p = static_cast<float>(i + 1) / static_cast<float>(shadow_cascade_count);
            float log = near_plane * vull::pow((near_plane + clip_range) / near_plane, p);
            float uniform = near_plane + clip_range * p;
            float d = split_lambda * (log - uniform) + uniform;
            split_distances[i] = (d - near_plane) / clip_range;
        }

        // Build cascade matrices.
        const auto inv_camera = vull::inverse(
            vull::perspective(window.aspect_ratio(), vull::half_pi<float>, near_plane, shadow_distance) * ubo.view);
        float last_split_distance = 0.0f;
        for (uint32_t i = 0; i < shadow_cascade_count; i++) {
            Array<Vec3f, 8> frustum_corners{
                Vec3f(-1.0f, 1.0f, -1.0f),  Vec3f(1.0f, 1.0f, -1.0f),  Vec3f(1.0f, -1.0f, -1.0f),
                Vec3f(-1.0f, -1.0f, -1.0f), Vec3f(-1.0f, 1.0f, 1.0f),  Vec3f(1.0f, 1.0f, 1.0f),
                Vec3f(1.0f, -1.0f, 1.0f),   Vec3f(-1.0f, -1.0f, 1.0f),
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
            radius = ceilf(radius * 16.0f) / 16.0f;

            // TODO: direction duplicated in shader.
            constexpr Vec3f direction(0.6f, 0.6f, -0.6f);
            constexpr Vec3f up(0.0f, 1.0f, 0.0f);
            auto proj = vull::ortho(-radius, radius, -radius, radius, 0.0f, radius * 2.0f);
            auto view = vull::look_at(frustum_center + direction * radius, frustum_center, up);

            // Apply a small correction factor to the projection matrix to snap texels and avoid shimmering around the
            // edges of shadows.
            Vec4f origin = (proj * view * Vec4f(0.0f, 0.0f, 0.0f, 1.0f)) * (shadow_resolution / 2.0f);
            Vec2f rounded_origin(roundf(origin.x()), roundf(origin.y()));
            Vec2f round_offset = (rounded_origin - origin) * (2.0f / shadow_resolution);
            proj[3] += Vec4f(round_offset, 0.0f, 0.0f);

            ubo.sun_matrices[i] = proj * view;
            ubo.sun_cascade_split_depths[i] = (near_plane + split_distances[i] * clip_range);
            last_split_distance = split_distances[i];
        }
    };

    void *lights_data;
    void *ubo_data;
    context.vkMapMemory(lights_buffer_memory, 0, vk::k_whole_size, 0, &lights_data);
    context.vkMapMemory(uniform_buffer_memory, 0, vk::k_whole_size, 0, &ubo_data);

    vk::QueryPoolCreateInfo query_pool_ci{
        .sType = vk::StructureType::QueryPoolCreateInfo,
        .queryType = vk::QueryType::Timestamp,
        .queryCount = 10,
    };
    vk::QueryPool query_pool;
    context.vkCreateQueryPool(&query_pool_ci, &query_pool);

    ui::Renderer ui(context, swapchain, ui_vertex_shader, ui_fragment_shader);
    ui::TimeGraph cpu_time_graph(Vec2f(600.0f, 300.0f), Vec3f(0.6f, 0.7f, 0.8f));
    ui::TimeGraph gpu_time_graph(Vec2f(600.0f, 300.0f), Vec3f(0.8f, 0.0f, 0.7f));
    auto font = ui.load_font("../engine/fonts/DejaVuSansMono.ttf", 20);
    ui.set_global_scale(window.ppcm() / 37.8f * 0.55f);

    vk::PhysicalDeviceProperties device_properties{};
    context.vkGetPhysicalDeviceProperties(&device_properties);

    Timer frame_timer;
    while (!window.should_close()) {
        float dt = frame_timer.elapsed();
        frame_timer.reset();

        ui::TimeGraph::Bar cpu_frame_bar;

        Timer acquire_timer;
        uint32_t image_index = swapchain.acquire_image(image_available_semaphore);
        cpu_frame_bar.sections.push({"Acquire swapchain", acquire_timer.elapsed()});

        Timer wait_fence_timer;
        context.vkWaitForFences(1, &fence, true, ~0ul);
        context.vkResetFences(1, &fence);
        cpu_frame_bar.sections.push({"Wait fence", wait_fence_timer.elapsed()});

        Array<uint64_t, 10> timestamp_data{};
        context.vkGetQueryPoolResults(query_pool, 0, timestamp_data.size(), timestamp_data.size_bytes(),
                                      timestamp_data.data(), sizeof(uint64_t), vk::QueryResultFlags::_64);

        ui::TimeGraph::Bar gpu_frame_bar;
        gpu_frame_bar.sections.push({"Depth pass", (static_cast<float>((timestamp_data[1] - timestamp_data[0])) *
                                                    device_properties.limits.timestampPeriod) /
                                                       1000000000.0f});
        gpu_frame_bar.sections.push({"Shadow pass", (static_cast<float>((timestamp_data[3] - timestamp_data[2])) *
                                                     device_properties.limits.timestampPeriod) /
                                                        1000000000.0f});
        gpu_frame_bar.sections.push({"Light cull", (static_cast<float>((timestamp_data[5] - timestamp_data[4])) *
                                                    device_properties.limits.timestampPeriod) /
                                                       1000000000.0f});
        gpu_frame_bar.sections.push({"Main pass", (static_cast<float>((timestamp_data[7] - timestamp_data[6])) *
                                                   device_properties.limits.timestampPeriod) /
                                                      1000000000.0f});
        gpu_frame_bar.sections.push({"UI", (static_cast<float>((timestamp_data[9] - timestamp_data[8])) *
                                            device_properties.limits.timestampPeriod) /
                                               1000000000.0f});
        gpu_time_graph.add_bar(move(gpu_frame_bar));

        ui.draw_rect(Vec4f(0.06f, 0.06f, 0.06f, 1.0f), {100.0f, 100.0f}, {1000.0f, 25.0f});
        ui.draw_rect(Vec4f(0.06f, 0.06f, 0.06f, 0.75f), {100.0f, 125.0f}, {1000.0f, 750.0f});
        cpu_time_graph.draw(ui, {120.0f, 200.0f}, font, "CPU time");
        gpu_time_graph.draw(ui, {120.0f, 550.0f}, font, "GPU time");
        ui.draw_text(font, {0.949f, 0.96f, 0.98f}, {95.0f, 140.0f},
                     vull::format("Camera position: ({}, {}, {})", ubo.camera_position.x(), ubo.camera_position.y(),
                                  ubo.camera_position.z()));

        camera.update(window, dt);
        ubo.camera_position = camera.position();
        ubo.view = camera.view_matrix();
        update_cascades();

        uint32_t light_count = lights.size();
        memcpy(lights_data, &light_count, sizeof(uint32_t));
        memcpy(reinterpret_cast<char *>(lights_data) + 4 * sizeof(float), lights.data(), lights.size_bytes());
        memcpy(ubo_data, &ubo, sizeof(UniformBuffer));

        Timer record_timer;
        const auto &cmd_buf = command_pool.request_cmd_buf();
        cmd_buf.reset_query_pool(query_pool, query_pool_ci.queryCount);
        cmd_buf.bind_descriptor_sets(vk::PipelineBindPoint::Compute, pipeline_layout, {&descriptor_set, 1});
        cmd_buf.bind_descriptor_sets(vk::PipelineBindPoint::Graphics, pipeline_layout, {&descriptor_set, 1});

        auto render_meshes = [&](uint32_t cascade_index) {
            for (auto [entity, mesh, material] : world.view<Mesh, Material>()) {
                PushConstantBlock push_constant_block{
                    .transform = get_transform_matrix(world, entity),
                    .albedo_index = material.albedo_index(),
                    .normal_index = material.normal_index(),
                    .cascade_index = cascade_index,
                };
                cmd_buf.bind_vertex_buffer(vertex_buffers[mesh.index()]);
                cmd_buf.bind_index_buffer(index_buffers[mesh.index()], vk::IndexType::Uint32);
                cmd_buf.push_constants(pipeline_layout, vk::ShaderStage::Vertex | vk::ShaderStage::Fragment,
                                       sizeof(PushConstantBlock), &push_constant_block);
                cmd_buf.draw_indexed(mesh.index_count(), 1);
            }
        };

        vk::ImageMemoryBarrier depth_write_barrier{
            .sType = vk::StructureType::ImageMemoryBarrier,
            .dstAccessMask = vk::Access::DepthStencilAttachmentWrite,
            .oldLayout = vk::ImageLayout::Undefined,
            .newLayout = vk::ImageLayout::DepthAttachmentOptimal,
            .image = depth_image,
            .subresourceRange{
                .aspectMask = vk::ImageAspect::Depth,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        cmd_buf.pipeline_barrier(vk::PipelineStage::TopOfPipe,
                                 vk::PipelineStage::EarlyFragmentTests | vk::PipelineStage::LateFragmentTests, {},
                                 {&depth_write_barrier, 1});

        vk::RenderingAttachmentInfo depth_write_attachment{
            .sType = vk::StructureType::RenderingAttachmentInfo,
            .imageView = depth_image_view,
            .imageLayout = vk::ImageLayout::DepthAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::Clear,
            .storeOp = vk::AttachmentStoreOp::Store,
            .clearValue{
                .depthStencil{0.0f, 0},
            },
        };
        vk::RenderingInfo depth_pass_rendering_info{
            .sType = vk::StructureType::RenderingInfo,
            .renderArea{
                .extent = swapchain.extent_2D(),
            },
            .layerCount = 1,
            .pDepthAttachment = &depth_write_attachment,
            .pStencilAttachment = &depth_write_attachment,
        };
        cmd_buf.write_timestamp(vk::PipelineStage::TopOfPipe, query_pool, 0);
        cmd_buf.begin_rendering(depth_pass_rendering_info);
        cmd_buf.bind_pipeline(vk::PipelineBindPoint::Graphics, depth_pass_pipeline);
        render_meshes(0);
        cmd_buf.end_rendering();

        vk::ImageMemoryBarrier shadow_map_write_barrier{
            .sType = vk::StructureType::ImageMemoryBarrier,
            .dstAccessMask = vk::Access::DepthStencilAttachmentWrite,
            .oldLayout = vk::ImageLayout::Undefined,
            .newLayout = vk::ImageLayout::DepthAttachmentOptimal,
            .image = shadow_map,
            .subresourceRange{
                .aspectMask = vk::ImageAspect::Depth,
                .levelCount = 1,
                .layerCount = shadow_cascade_count,
            },
        };
        cmd_buf.pipeline_barrier(vk::PipelineStage::TopOfPipe,
                                 vk::PipelineStage::EarlyFragmentTests | vk::PipelineStage::LateFragmentTests, {},
                                 {&shadow_map_write_barrier, 1});
        cmd_buf.write_timestamp(vk::PipelineStage::AllGraphics, query_pool, 1);

        cmd_buf.write_timestamp(vk::PipelineStage::TopOfPipe, query_pool, 2);
        cmd_buf.bind_pipeline(vk::PipelineBindPoint::Graphics, shadow_pass_pipeline);
        for (uint32_t i = 0; i < shadow_cascade_count; i++) {
            vk::RenderingAttachmentInfo shadow_map_write_attachment{
                .sType = vk::StructureType::RenderingAttachmentInfo,
                .imageView = shadow_cascade_views[i],
                .imageLayout = vk::ImageLayout::DepthAttachmentOptimal,
                .loadOp = vk::AttachmentLoadOp::Clear,
                .storeOp = vk::AttachmentStoreOp::Store,
                .clearValue{
                    .depthStencil{1.0f, 0},
                },
            };
            vk::RenderingInfo shadow_map_rendering_info{
                .sType = vk::StructureType::RenderingInfo,
                .renderArea{
                    .extent = {shadow_resolution, shadow_resolution},
                },
                .layerCount = 1,
                .pDepthAttachment = &shadow_map_write_attachment,
                .pStencilAttachment = &shadow_map_write_attachment,
            };
            cmd_buf.begin_rendering(shadow_map_rendering_info);
            render_meshes(i);
            cmd_buf.end_rendering();
        }

        vk::ImageMemoryBarrier depth_sample_barrier{
            .sType = vk::StructureType::ImageMemoryBarrier,
            .srcAccessMask = vk::Access::DepthStencilAttachmentWrite,
            .dstAccessMask = vk::Access::ShaderRead,
            .oldLayout = vk::ImageLayout::DepthAttachmentOptimal,
            .newLayout = vk::ImageLayout::ShaderReadOnlyOptimal,
            .image = depth_image,
            .subresourceRange{
                .aspectMask = vk::ImageAspect::Depth,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        cmd_buf.pipeline_barrier(vk::PipelineStage::EarlyFragmentTests | vk::PipelineStage::LateFragmentTests,
                                 vk::PipelineStage::ComputeShader, {}, {&depth_sample_barrier, 1});
        cmd_buf.write_timestamp(vk::PipelineStage::AllGraphics, query_pool, 3);
        cmd_buf.bind_pipeline(vk::PipelineBindPoint::Compute, light_cull_pipeline);
        cmd_buf.dispatch(row_tile_count, col_tile_count, 1);
        cmd_buf.write_timestamp(vk::PipelineStage::TopOfPipe, query_pool, 4);
        cmd_buf.write_timestamp(vk::PipelineStage::ComputeShader, query_pool, 5);

        Array main_pass_buffer_barriers{
            vk::BufferMemoryBarrier{
                .sType = vk::StructureType::BufferMemoryBarrier,
                .srcAccessMask = vk::Access::ShaderWrite,
                .dstAccessMask = vk::Access::ShaderRead,
                .buffer = lights_buffer,
                .size = lights_buffer_size,
            },
            vk::BufferMemoryBarrier{
                .sType = vk::StructureType::BufferMemoryBarrier,
                .srcAccessMask = vk::Access::ShaderWrite,
                .dstAccessMask = vk::Access::ShaderRead,
                .buffer = light_visibilities_buffer,
                .size = light_visibilities_buffer_size,
            },
        };
        cmd_buf.pipeline_barrier(vk::PipelineStage::ComputeShader, vk::PipelineStage::FragmentShader,
                                 main_pass_buffer_barriers.span(), {});

        vk::ImageMemoryBarrier colour_write_barrier{
            .sType = vk::StructureType::ImageMemoryBarrier,
            .dstAccessMask = vk::Access::ColorAttachmentWrite,
            .oldLayout = vk::ImageLayout::Undefined,
            .newLayout = vk::ImageLayout::ColorAttachmentOptimal,
            .image = swapchain.image(image_index),
            .subresourceRange{
                .aspectMask = vk::ImageAspect::Color,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        vk::ImageMemoryBarrier depth_read_barrier{
            .sType = vk::StructureType::ImageMemoryBarrier,
            .srcAccessMask = vk::Access::ShaderRead,
            .dstAccessMask = vk::Access::DepthStencilAttachmentRead,
            .oldLayout = vk::ImageLayout::ShaderReadOnlyOptimal,
            .newLayout = vk::ImageLayout::DepthReadOnlyOptimal,
            .image = depth_image,
            .subresourceRange{
                .aspectMask = vk::ImageAspect::Depth,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        vk::ImageMemoryBarrier shadow_map_sample_barrier{
            .sType = vk::StructureType::ImageMemoryBarrier,
            .srcAccessMask = vk::Access::DepthStencilAttachmentWrite,
            .dstAccessMask = vk::Access::ShaderRead,
            .oldLayout = vk::ImageLayout::DepthAttachmentOptimal,
            .newLayout = vk::ImageLayout::ShaderReadOnlyOptimal,
            .image = shadow_map,
            .subresourceRange{
                .aspectMask = vk::ImageAspect::Depth,
                .levelCount = 1,
                .layerCount = shadow_cascade_count,
            },
        };
        cmd_buf.pipeline_barrier(vk::PipelineStage::TopOfPipe, vk::PipelineStage::ColorAttachmentOutput, {},
                                 {&colour_write_barrier, 1});
        cmd_buf.pipeline_barrier(vk::PipelineStage::ComputeShader,
                                 vk::PipelineStage::EarlyFragmentTests | vk::PipelineStage::LateFragmentTests, {},
                                 {&depth_read_barrier, 1});
        cmd_buf.pipeline_barrier(vk::PipelineStage::EarlyFragmentTests | vk::PipelineStage::LateFragmentTests,
                                 vk::PipelineStage::FragmentShader, {}, {&shadow_map_sample_barrier, 1});

        vk::RenderingAttachmentInfo colour_write_attachment{
            .sType = vk::StructureType::RenderingAttachmentInfo,
            .imageView = swapchain.image_view(image_index),
            .imageLayout = vk::ImageLayout::ColorAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::Clear,
            .storeOp = vk::AttachmentStoreOp::Store,
            .clearValue{
                .color{{0.47f, 0.5f, 0.67f, 1.0f}},
            },
        };
        vk::RenderingAttachmentInfo depth_read_attachment{
            .sType = vk::StructureType::RenderingAttachmentInfo,
            .imageView = depth_image_view,
            .imageLayout = vk::ImageLayout::DepthReadOnlyOptimal,
            .loadOp = vk::AttachmentLoadOp::Load,
            .storeOp = vk::AttachmentStoreOp::None,
        };
        vk::RenderingInfo main_pass_rendering_info{
            .sType = vk::StructureType::RenderingInfo,
            .renderArea{
                .extent = swapchain.extent_2D(),
            },
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colour_write_attachment,
            .pDepthAttachment = &depth_read_attachment,
            .pStencilAttachment = &depth_read_attachment,
        };
        cmd_buf.write_timestamp(vk::PipelineStage::TopOfPipe, query_pool, 6);
        cmd_buf.begin_rendering(main_pass_rendering_info);
        cmd_buf.bind_pipeline(vk::PipelineBindPoint::Graphics, main_pass_pipeline);
        render_meshes(0);
        cmd_buf.end_rendering();
        cmd_buf.write_timestamp(vk::PipelineStage::AllGraphics, query_pool, 7);

        vk::ImageMemoryBarrier ui_colour_write_barrier{
            .sType = vk::StructureType::ImageMemoryBarrier,
            .srcAccessMask = vk::Access::ColorAttachmentWrite,
            .dstAccessMask = vk::Access::ColorAttachmentRead,
            .oldLayout = vk::ImageLayout::ColorAttachmentOptimal,
            .newLayout = vk::ImageLayout::ColorAttachmentOptimal,
            .image = swapchain.image(image_index),
            .subresourceRange{
                .aspectMask = vk::ImageAspect::Color,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        cmd_buf.pipeline_barrier(vk::PipelineStage::ColorAttachmentOutput, vk::PipelineStage::ColorAttachmentOutput, {},
                                 {&ui_colour_write_barrier, 1});

        cmd_buf.write_timestamp(vk::PipelineStage::ColorAttachmentOutput, query_pool, 8);
        ui.render(cmd_buf, image_index);
        cmd_buf.write_timestamp(vk::PipelineStage::AllGraphics, query_pool, 9);

        vk::ImageMemoryBarrier colour_present_barrier{
            .sType = vk::StructureType::ImageMemoryBarrier,
            .srcAccessMask = vk::Access::ColorAttachmentWrite,
            .oldLayout = vk::ImageLayout::ColorAttachmentOptimal,
            .newLayout = vk::ImageLayout::PresentSrcKHR,
            .image = swapchain.image(image_index),
            .subresourceRange{
                .aspectMask = vk::ImageAspect::Color,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        cmd_buf.pipeline_barrier(vk::PipelineStage::ColorAttachmentOutput, vk::PipelineStage::BottomOfPipe, {},
                                 {&colour_present_barrier, 1});

        Array signal_semaphores{
            vk::SemaphoreSubmitInfo{
                .sType = vk::StructureType::SemaphoreSubmitInfo,
                .semaphore = rendering_finished_semaphore,
            },
        };
        Array wait_semaphores{
            vk::SemaphoreSubmitInfo{
                .sType = vk::StructureType::SemaphoreSubmitInfo,
                .semaphore = image_available_semaphore,
                .stageMask = static_cast<vk::PipelineStageFlags2>(vk::PipelineStage::ColorAttachmentOutput),
            },
        };
        queue.submit(cmd_buf, fence, signal_semaphores.span(), wait_semaphores.span());
        cpu_frame_bar.sections.push({"Record", record_timer.elapsed()});

        Array present_wait_semaphores{rendering_finished_semaphore};
        swapchain.present(image_index, present_wait_semaphores.span());
        window.poll_events();
        cpu_time_graph.add_bar(move(cpu_frame_bar));
    }
    scheduler.stop();
    context.vkDeviceWaitIdle();
    context.vkDestroyQueryPool(query_pool);
    context.vkDestroySemaphore(rendering_finished_semaphore);
    context.vkDestroySemaphore(image_available_semaphore);
    context.vkDestroyFence(fence);
    context.vkDestroyDescriptorPool(descriptor_pool);
    context.vkFreeMemory(light_visibilities_buffer_memory);
    context.vkDestroyBuffer(light_visibilities_buffer);
    context.vkFreeMemory(lights_buffer_memory);
    context.vkDestroyBuffer(lights_buffer);
    context.vkFreeMemory(uniform_buffer_memory);
    context.vkDestroyBuffer(uniform_buffer);
    context.vkDestroySampler(normal_sampler);
    context.vkDestroySampler(albedo_sampler);
    context.vkDestroySampler(depth_sampler);
    for (auto *cascade_view : shadow_cascade_views) {
        context.vkDestroyImageView(cascade_view);
    }
    context.vkDestroyImageView(shadow_map_view);
    context.vkFreeMemory(shadow_map_memory);
    context.vkDestroyImage(shadow_map);
    context.vkDestroyImageView(depth_image_view);
    context.vkFreeMemory(depth_image_memory);
    context.vkDestroyImage(depth_image);
    context.vkDestroyPipeline(main_pass_pipeline);
    context.vkDestroyPipeline(light_cull_pipeline);
    context.vkDestroyPipeline(shadow_pass_pipeline);
    context.vkDestroyPipeline(depth_pass_pipeline);
    context.vkDestroyPipelineLayout(pipeline_layout);
    context.vkDestroyDescriptorSetLayout(set_layout);
    context.vkDestroyShaderModule(ui_fragment_shader);
    context.vkDestroyShaderModule(ui_vertex_shader);
    context.vkDestroyShaderModule(shadow_shader);
    context.vkDestroyShaderModule(main_fragment_shader);
    context.vkDestroyShaderModule(main_vertex_shader);
    context.vkDestroyShaderModule(light_cull_shader);
    for (auto *image_view : texture_image_views) {
        context.vkDestroyImageView(image_view);
    }
    for (auto *image : texture_images) {
        context.vkDestroyImage(image);
    }
    for (auto *buffer : index_buffers) {
        context.vkDestroyBuffer(buffer);
    }
    for (auto *buffer : vertex_buffers) {
        context.vkDestroyBuffer(buffer);
    }
    context.vkFreeMemory(scene_memory);
}

} // namespace

int main() {
    Scheduler scheduler;
    scheduler.start([&] {
        main_task(scheduler);
    });
}
