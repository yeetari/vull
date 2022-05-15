#include "Camera.hh"

#include <vull/core/Scene.hh>
#include <vull/core/Vertex.hh>
#include <vull/core/Window.hh>
#include <vull/maths/Common.hh>
#include <vull/maths/Mat.hh>
#include <vull/maths/Vec.hh>
#include <vull/support/Array.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Format.hh>
#include <vull/support/String.hh>
#include <vull/support/Timer.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>
#include <vull/tasklet/Scheduler.hh>
#include <vull/tasklet/Tasklet.hh> // IWYU pragma: keep
#include <vull/ui/Renderer.hh>
#include <vull/ui/TimeGraph.hh>
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
        if ((family.queueFlags & vkb::QueueFlags::Graphics) != vkb::QueueFlags::None) {
            return i;
        }
    }
    VULL_ENSURE_NOT_REACHED();
}

vkb::ShaderModule load_shader(const VkContext &context, const char *path) {
    FILE *file = fopen(path, "rb");
    fseek(file, 0, SEEK_END);
    LargeVector<uint32_t> binary(static_cast<size_t>(ftell(file)) / sizeof(uint32_t));
    fseek(file, 0, SEEK_SET);
    VULL_ENSURE(fread(binary.data(), sizeof(uint32_t), binary.size(), file) == binary.size());
    fclose(file);
    vkb::ShaderModuleCreateInfo module_ci{
        .sType = vkb::StructureType::ShaderModuleCreateInfo,
        .codeSize = binary.size_bytes(),
        .pCode = binary.data(),
    };
    vkb::ShaderModule module;
    VULL_ENSURE(context.vkCreateShaderModule(&module_ci, &module) == vkb::Result::Success);
    return module;
}

void main_task(Scheduler &scheduler) {
    Window window(2560, 1440, true);
    VkContext context;
    auto swapchain = window.create_swapchain(context);

    const auto graphics_family_index = find_graphics_family(context);
    CommandPool cmd_pool(context, graphics_family_index);
    Queue queue(context, graphics_family_index);

    auto *pack_file = fopen("scene.vpak", "rb");
    Scene scene(context);
    scene.load(cmd_pool, queue, pack_file);
    fclose(pack_file);

    constexpr uint32_t tile_size = 32;
    uint32_t row_tile_count = (window.width() + (window.width() % tile_size)) / tile_size;
    uint32_t col_tile_count = (window.height() + (window.height() % tile_size)) / tile_size;

    struct SpecialisationData {
        uint32_t viewport_width;
        uint32_t viewport_height;
        uint32_t tile_size;
        uint32_t tile_max_light_count;
        uint32_t row_tile_count;
    } specialisation_data{
        .viewport_width = window.width(),
        .viewport_height = window.height(),
        .tile_size = tile_size,
        .tile_max_light_count = 400,
        .row_tile_count = row_tile_count,
    };

    Array specialisation_map_entries{
        vkb::SpecializationMapEntry{
            .constantID = 0,
            .offset = offsetof(SpecialisationData, viewport_width),
            .size = sizeof(SpecialisationData::viewport_width),
        },
        vkb::SpecializationMapEntry{
            .constantID = 1,
            .offset = offsetof(SpecialisationData, viewport_height),
            .size = sizeof(SpecialisationData::viewport_height),
        },
        vkb::SpecializationMapEntry{
            .constantID = 2,
            .offset = offsetof(SpecialisationData, tile_size),
            .size = sizeof(SpecialisationData::tile_size),
        },
        vkb::SpecializationMapEntry{
            .constantID = 3,
            .offset = offsetof(SpecialisationData, tile_max_light_count),
            .size = sizeof(SpecialisationData::tile_max_light_count),
        },
        vkb::SpecializationMapEntry{
            .constantID = 4,
            .offset = offsetof(SpecialisationData, row_tile_count),
            .size = sizeof(SpecialisationData::row_tile_count),
        },
    };
    vkb::SpecializationInfo specialisation_info{
        .mapEntryCount = specialisation_map_entries.size(),
        .pMapEntries = specialisation_map_entries.data(),
        .dataSize = sizeof(SpecialisationData),
        .pData = &specialisation_data,
    };

    auto *default_vertex_shader = load_shader(context, "engine/shaders/default.vert.spv");
    auto *default_fragment_shader = load_shader(context, "engine/shaders/default.frag.spv");
    auto *deferred_shader = load_shader(context, "engine/shaders/deferred.comp.spv");
    auto *light_cull_shader = load_shader(context, "engine/shaders/light_cull.comp.spv");
    auto *shadow_shader = load_shader(context, "engine/shaders/shadow.vert.spv");
    auto *ui_vertex_shader = load_shader(context, "engine/shaders/ui.vert.spv");
    auto *ui_fragment_shader = load_shader(context, "engine/shaders/ui.frag.spv");

    Array geometry_pass_shader_stage_cis{
        vkb::PipelineShaderStageCreateInfo{
            .sType = vkb::StructureType::PipelineShaderStageCreateInfo,
            .stage = vkb::ShaderStage::Vertex,
            .module = default_vertex_shader,
            .pName = "main",
            .pSpecializationInfo = &specialisation_info,
        },
        vkb::PipelineShaderStageCreateInfo{
            .sType = vkb::StructureType::PipelineShaderStageCreateInfo,
            .stage = vkb::ShaderStage::Fragment,
            .module = default_fragment_shader,
            .pName = "main",
            .pSpecializationInfo = &specialisation_info,
        },
    };
    vkb::PipelineShaderStageCreateInfo deferred_shader_stage_ci{
        .sType = vkb::StructureType::PipelineShaderStageCreateInfo,
        .stage = vkb::ShaderStage::Compute,
        .module = deferred_shader,
        .pName = "main",
        .pSpecializationInfo = &specialisation_info,
    };
    vkb::PipelineShaderStageCreateInfo light_cull_shader_stage_ci{
        .sType = vkb::StructureType::PipelineShaderStageCreateInfo,
        .stage = vkb::ShaderStage::Compute,
        .module = light_cull_shader,
        .pName = "main",
        .pSpecializationInfo = &specialisation_info,
    };
    vkb::PipelineShaderStageCreateInfo shadow_shader_stage_ci{
        .sType = vkb::StructureType::PipelineShaderStageCreateInfo,
        .stage = vkb::ShaderStage::Vertex,
        .module = shadow_shader,
        .pName = "main",
        .pSpecializationInfo = &specialisation_info,
    };

    Array global_set_bindings{
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
            .descriptorType = vkb::DescriptorType::StorageBuffer,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Compute,
        },
        vkb::DescriptorSetLayoutBinding{
            .binding = 3,
            .descriptorType = vkb::DescriptorType::StorageImage,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Compute,
        },
    };
    vkb::DescriptorSetLayoutCreateInfo global_set_layout_ci{
        .sType = vkb::StructureType::DescriptorSetLayoutCreateInfo,
        .bindingCount = global_set_bindings.size(),
        .pBindings = global_set_bindings.data(),
    };
    vkb::DescriptorSetLayout global_set_layout;
    VULL_ENSURE(context.vkCreateDescriptorSetLayout(&global_set_layout_ci, &global_set_layout) == vkb::Result::Success);

    Array geometry_set_bindings{
        vkb::DescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = vkb::DescriptorType::Sampler,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Fragment,
        },
        vkb::DescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = vkb::DescriptorType::Sampler,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Fragment,
        },
        vkb::DescriptorSetLayoutBinding{
            .binding = 2,
            .descriptorType = vkb::DescriptorType::SampledImage,
            .descriptorCount = scene.texture_count(),
            .stageFlags = vkb::ShaderStage::Fragment,
        },
    };
    vkb::DescriptorSetLayoutCreateInfo geometry_set_layout_ci{
        .sType = vkb::StructureType::DescriptorSetLayoutCreateInfo,
        .bindingCount = geometry_set_bindings.size(),
        .pBindings = geometry_set_bindings.data(),
    };
    vkb::DescriptorSetLayout geometry_set_layout;
    VULL_ENSURE(context.vkCreateDescriptorSetLayout(&geometry_set_layout_ci, &geometry_set_layout) ==
                vkb::Result::Success);

    Array deferred_set_bindings{
        vkb::DescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = vkb::DescriptorType::CombinedImageSampler,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Compute,
        },
        vkb::DescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = vkb::DescriptorType::CombinedImageSampler,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Compute,
        },
        vkb::DescriptorSetLayoutBinding{
            .binding = 2,
            .descriptorType = vkb::DescriptorType::CombinedImageSampler,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Compute,
        },
        vkb::DescriptorSetLayoutBinding{
            .binding = 3,
            .descriptorType = vkb::DescriptorType::CombinedImageSampler,
            .descriptorCount = 1,
            .stageFlags = vkb::ShaderStage::Compute,
        },
    };
    vkb::DescriptorSetLayoutCreateInfo deferred_set_layout_ci{
        .sType = vkb::StructureType::DescriptorSetLayoutCreateInfo,
        .bindingCount = deferred_set_bindings.size(),
        .pBindings = deferred_set_bindings.data(),
    };
    vkb::DescriptorSetLayout deferred_set_layout;
    VULL_ENSURE(context.vkCreateDescriptorSetLayout(&deferred_set_layout_ci, &deferred_set_layout) ==
                vkb::Result::Success);

    vkb::PushConstantRange push_constant_range{
        .stageFlags = vkb::ShaderStage::All,
        .size = sizeof(PushConstantBlock),
    };
    Array geometry_set_layouts{
        global_set_layout,
        geometry_set_layout,
    };
    vkb::PipelineLayoutCreateInfo geometry_pipeline_layout_ci{
        .sType = vkb::StructureType::PipelineLayoutCreateInfo,
        .setLayoutCount = geometry_set_layouts.size(),
        .pSetLayouts = geometry_set_layouts.data(),
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_constant_range,
    };
    vkb::PipelineLayout geometry_pipeline_layout;
    VULL_ENSURE(context.vkCreatePipelineLayout(&geometry_pipeline_layout_ci, &geometry_pipeline_layout) ==
                vkb::Result::Success);

    Array compute_set_layouts{
        global_set_layout,
        deferred_set_layout,
    };
    vkb::PipelineLayoutCreateInfo compute_pipeline_layout_ci{
        .sType = vkb::StructureType::PipelineLayoutCreateInfo,
        .setLayoutCount = compute_set_layouts.size(),
        .pSetLayouts = compute_set_layouts.data(),
    };
    vkb::PipelineLayout compute_pipeline_layout;
    VULL_ENSURE(context.vkCreatePipelineLayout(&compute_pipeline_layout_ci, &compute_pipeline_layout) ==
                vkb::Result::Success);

    Array vertex_attribute_descriptions{
        vkb::VertexInputAttributeDescription{
            .location = 0,
            .format = vkb::Format::R32G32B32Sfloat,
            .offset = offsetof(Vertex, position),
        },
        vkb::VertexInputAttributeDescription{
            .location = 1,
            .format = vkb::Format::R32G32B32Sfloat,
            .offset = offsetof(Vertex, normal),
        },
        vkb::VertexInputAttributeDescription{
            .location = 2,
            .format = vkb::Format::R32G32Sfloat,
            .offset = offsetof(Vertex, uv),
        },
    };
    vkb::VertexInputBindingDescription vertex_binding_description{
        .stride = sizeof(Vertex),
        .inputRate = vkb::VertexInputRate::Vertex,
    };
    vkb::PipelineVertexInputStateCreateInfo main_vertex_input_state{
        .sType = vkb::StructureType::PipelineVertexInputStateCreateInfo,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &vertex_binding_description,
        .vertexAttributeDescriptionCount = vertex_attribute_descriptions.size(),
        .pVertexAttributeDescriptions = vertex_attribute_descriptions.data(),
    };
    vkb::PipelineVertexInputStateCreateInfo shadow_vertex_input_state{
        .sType = vkb::StructureType::PipelineVertexInputStateCreateInfo,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &vertex_binding_description,
        .vertexAttributeDescriptionCount = 1,
        .pVertexAttributeDescriptions = &vertex_attribute_descriptions.first(),
    };
    vkb::PipelineInputAssemblyStateCreateInfo input_assembly_state{
        .sType = vkb::StructureType::PipelineInputAssemblyStateCreateInfo,
        .topology = vkb::PrimitiveTopology::TriangleList,
    };

    vkb::Rect2D scissor{
        .extent = swapchain.extent_2D(),
    };
    vkb::Viewport viewport{
        .width = static_cast<float>(window.width()),
        .height = static_cast<float>(window.height()),
        .maxDepth = 1.0f,
    };
    vkb::PipelineViewportStateCreateInfo viewport_state{
        .sType = vkb::StructureType::PipelineViewportStateCreateInfo,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor,
    };

    constexpr uint32_t shadow_resolution = 2048;
    vkb::Rect2D shadow_scissor{
        .extent = {shadow_resolution, shadow_resolution},
    };
    vkb::Viewport shadow_viewport{
        .width = shadow_resolution,
        .height = shadow_resolution,
        .maxDepth = 1.0f,
    };
    vkb::PipelineViewportStateCreateInfo shadow_viewport_state{
        .sType = vkb::StructureType::PipelineViewportStateCreateInfo,
        .viewportCount = 1,
        .pViewports = &shadow_viewport,
        .scissorCount = 1,
        .pScissors = &shadow_scissor,
    };

    vkb::PipelineRasterizationStateCreateInfo main_rasterisation_state{
        .sType = vkb::StructureType::PipelineRasterizationStateCreateInfo,
        .polygonMode = vkb::PolygonMode::Fill,
        .cullMode = vkb::CullMode::Back,
        .frontFace = vkb::FrontFace::CounterClockwise,
        .lineWidth = 1.0f,
    };
    vkb::PipelineRasterizationStateCreateInfo shadow_rasterisation_state{
        .sType = vkb::StructureType::PipelineRasterizationStateCreateInfo,
        .polygonMode = vkb::PolygonMode::Fill,
        .cullMode = vkb::CullMode::Back,
        .frontFace = vkb::FrontFace::CounterClockwise,
        .depthBiasEnable = true,
        .depthBiasConstantFactor = 2.0f,
        .depthBiasSlopeFactor = 5.0f,
        .lineWidth = 1.0f,
    };

    vkb::PipelineMultisampleStateCreateInfo multisample_state{
        .sType = vkb::StructureType::PipelineMultisampleStateCreateInfo,
        .rasterizationSamples = vkb::SampleCount::_1,
        .minSampleShading = 1.0f,
    };

    vkb::PipelineDepthStencilStateCreateInfo main_depth_stencil_state{
        .sType = vkb::StructureType::PipelineDepthStencilStateCreateInfo,
        .depthTestEnable = true,
        .depthWriteEnable = true,
        .depthCompareOp = vkb::CompareOp::GreaterOrEqual,
    };
    vkb::PipelineDepthStencilStateCreateInfo shadow_depth_stencil_state{
        .sType = vkb::StructureType::PipelineDepthStencilStateCreateInfo,
        .depthTestEnable = true,
        .depthWriteEnable = true,
        .depthCompareOp = vkb::CompareOp::LessOrEqual,
    };

    Array main_blend_attachments{
        vkb::PipelineColorBlendAttachmentState{
            .colorWriteMask =
                vkb::ColorComponent::R | vkb::ColorComponent::G | vkb::ColorComponent::B | vkb::ColorComponent::A,
        },
        vkb::PipelineColorBlendAttachmentState{
            .colorWriteMask =
                vkb::ColorComponent::R | vkb::ColorComponent::G | vkb::ColorComponent::B | vkb::ColorComponent::A,
        },
    };
    vkb::PipelineColorBlendStateCreateInfo main_blend_state{
        .sType = vkb::StructureType::PipelineColorBlendStateCreateInfo,
        .attachmentCount = main_blend_attachments.size(),
        .pAttachments = main_blend_attachments.data(),
    };

    Array gbuffer_formats{
        vkb::Format::R8G8B8A8Unorm,
        vkb::Format::R32G32B32A32Sfloat,
    };
    const auto depth_format = vkb::Format::D32Sfloat;
    vkb::PipelineRenderingCreateInfo geometry_pass_rendering_create_info{
        .sType = vkb::StructureType::PipelineRenderingCreateInfo,
        .colorAttachmentCount = gbuffer_formats.size(),
        .pColorAttachmentFormats = gbuffer_formats.data(),
        .depthAttachmentFormat = depth_format,
        .stencilAttachmentFormat = depth_format,
    };
    vkb::GraphicsPipelineCreateInfo geometry_pass_pipeline_ci{
        .sType = vkb::StructureType::GraphicsPipelineCreateInfo,
        .pNext = &geometry_pass_rendering_create_info,
        .stageCount = geometry_pass_shader_stage_cis.size(),
        .pStages = geometry_pass_shader_stage_cis.data(),
        .pVertexInputState = &main_vertex_input_state,
        .pInputAssemblyState = &input_assembly_state,
        .pViewportState = &viewport_state,
        .pRasterizationState = &main_rasterisation_state,
        .pMultisampleState = &multisample_state,
        .pDepthStencilState = &main_depth_stencil_state,
        .pColorBlendState = &main_blend_state,
        .layout = geometry_pipeline_layout,
    };
    vkb::Pipeline geometry_pass_pipeline;
    VULL_ENSURE(context.vkCreateGraphicsPipelines(nullptr, 1, &geometry_pass_pipeline_ci, &geometry_pass_pipeline) ==
                vkb::Result::Success);

    vkb::PipelineRenderingCreateInfo shadow_pass_rendering_create_info{
        .sType = vkb::StructureType::PipelineRenderingCreateInfo,
        .depthAttachmentFormat = vkb::Format::D32Sfloat,
        .stencilAttachmentFormat = vkb::Format::D32Sfloat,
    };
    vkb::GraphicsPipelineCreateInfo shadow_pass_pipeline_ci{
        .sType = vkb::StructureType::GraphicsPipelineCreateInfo,
        .pNext = &shadow_pass_rendering_create_info,
        .stageCount = 1,
        .pStages = &shadow_shader_stage_ci,
        .pVertexInputState = &shadow_vertex_input_state,
        .pInputAssemblyState = &input_assembly_state,
        .pViewportState = &shadow_viewport_state,
        .pRasterizationState = &shadow_rasterisation_state,
        .pMultisampleState = &multisample_state,
        .pDepthStencilState = &shadow_depth_stencil_state,
        .layout = geometry_pipeline_layout,
    };
    vkb::Pipeline shadow_pass_pipeline;
    VULL_ENSURE(context.vkCreateGraphicsPipelines(nullptr, 1, &shadow_pass_pipeline_ci, &shadow_pass_pipeline) ==
                vkb::Result::Success);

    vkb::ComputePipelineCreateInfo light_cull_pipeline_ci{
        .sType = vkb::StructureType::ComputePipelineCreateInfo,
        .stage = light_cull_shader_stage_ci,
        .layout = compute_pipeline_layout,
    };
    vkb::Pipeline light_cull_pipeline;
    VULL_ENSURE(context.vkCreateComputePipelines(nullptr, 1, &light_cull_pipeline_ci, &light_cull_pipeline) ==
                vkb::Result::Success);

    vkb::ComputePipelineCreateInfo deferred_pipeline_ci{
        .sType = vkb::StructureType::ComputePipelineCreateInfo,
        .stage = deferred_shader_stage_ci,
        .layout = compute_pipeline_layout,
    };
    vkb::Pipeline deferred_pipeline;
    VULL_ENSURE(context.vkCreateComputePipelines(nullptr, 1, &deferred_pipeline_ci, &deferred_pipeline) ==
                vkb::Result::Success);

    vkb::ImageCreateInfo depth_image_ci{
        .sType = vkb::StructureType::ImageCreateInfo,
        .imageType = vkb::ImageType::_2D,
        .format = depth_format,
        .extent = swapchain.extent_3D(),
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = vkb::SampleCount::_1,
        .tiling = vkb::ImageTiling::Optimal,
        .usage = vkb::ImageUsage::DepthStencilAttachment | vkb::ImageUsage::Sampled,
        .sharingMode = vkb::SharingMode::Exclusive,
        .initialLayout = vkb::ImageLayout::Undefined,
    };
    vkb::Image depth_image;
    VULL_ENSURE(context.vkCreateImage(&depth_image_ci, &depth_image) == vkb::Result::Success);

    vkb::MemoryRequirements depth_image_requirements{};
    context.vkGetImageMemoryRequirements(depth_image, &depth_image_requirements);
    vkb::DeviceMemory depth_image_memory = context.allocate_memory(depth_image_requirements, MemoryType::DeviceLocal);
    VULL_ENSURE(context.vkBindImageMemory(depth_image, depth_image_memory, 0) == vkb::Result::Success);

    vkb::ImageViewCreateInfo depth_image_view_ci{
        .sType = vkb::StructureType::ImageViewCreateInfo,
        .image = depth_image,
        .viewType = vkb::ImageViewType::_2D,
        .format = depth_format,
        .subresourceRange{
            .aspectMask = vkb::ImageAspect::Depth,
            .levelCount = 1,
            .layerCount = 1,
        },
    };
    vkb::ImageView depth_image_view;
    VULL_ENSURE(context.vkCreateImageView(&depth_image_view_ci, &depth_image_view) == vkb::Result::Success);

    vkb::ImageCreateInfo albedo_image_ci{
        .sType = vkb::StructureType::ImageCreateInfo,
        .imageType = vkb::ImageType::_2D,
        .format = gbuffer_formats[0],
        .extent = swapchain.extent_3D(),
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = vkb::SampleCount::_1,
        .tiling = vkb::ImageTiling::Optimal,
        .usage = vkb::ImageUsage::ColorAttachment | vkb::ImageUsage::Sampled,
        .sharingMode = vkb::SharingMode::Exclusive,
        .initialLayout = vkb::ImageLayout::Undefined,
    };
    vkb::Image albedo_image;
    VULL_ENSURE(context.vkCreateImage(&albedo_image_ci, &albedo_image) == vkb::Result::Success);

    vkb::MemoryRequirements albedo_image_requirements{};
    context.vkGetImageMemoryRequirements(albedo_image, &albedo_image_requirements);
    vkb::DeviceMemory albedo_image_memory = context.allocate_memory(albedo_image_requirements, MemoryType::DeviceLocal);
    VULL_ENSURE(context.vkBindImageMemory(albedo_image, albedo_image_memory, 0) == vkb::Result::Success);

    vkb::ImageViewCreateInfo albedo_image_view_ci{
        .sType = vkb::StructureType::ImageViewCreateInfo,
        .image = albedo_image,
        .viewType = vkb::ImageViewType::_2D,
        .format = albedo_image_ci.format,
        .subresourceRange{
            .aspectMask = vkb::ImageAspect::Color,
            .levelCount = 1,
            .layerCount = 1,
        },
    };
    vkb::ImageView albedo_image_view;
    VULL_ENSURE(context.vkCreateImageView(&albedo_image_view_ci, &albedo_image_view) == vkb::Result::Success);

    vkb::ImageCreateInfo normal_image_ci{
        .sType = vkb::StructureType::ImageCreateInfo,
        .imageType = vkb::ImageType::_2D,
        .format = gbuffer_formats[1],
        .extent = swapchain.extent_3D(),
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = vkb::SampleCount::_1,
        .tiling = vkb::ImageTiling::Optimal,
        .usage = vkb::ImageUsage::ColorAttachment | vkb::ImageUsage::Sampled,
        .sharingMode = vkb::SharingMode::Exclusive,
        .initialLayout = vkb::ImageLayout::Undefined,
    };
    vkb::Image normal_image;
    VULL_ENSURE(context.vkCreateImage(&normal_image_ci, &normal_image) == vkb::Result::Success);

    vkb::MemoryRequirements normal_image_requirements{};
    context.vkGetImageMemoryRequirements(normal_image, &normal_image_requirements);
    vkb::DeviceMemory normal_image_memory = context.allocate_memory(normal_image_requirements, MemoryType::DeviceLocal);
    VULL_ENSURE(context.vkBindImageMemory(normal_image, normal_image_memory, 0) == vkb::Result::Success);

    vkb::ImageViewCreateInfo normal_image_view_ci{
        .sType = vkb::StructureType::ImageViewCreateInfo,
        .image = normal_image,
        .viewType = vkb::ImageViewType::_2D,
        .format = normal_image_ci.format,
        .subresourceRange{
            .aspectMask = vkb::ImageAspect::Color,
            .levelCount = 1,
            .layerCount = 1,
        },
    };
    vkb::ImageView normal_image_view;
    VULL_ENSURE(context.vkCreateImageView(&normal_image_view_ci, &normal_image_view) == vkb::Result::Success);

    constexpr uint32_t shadow_cascade_count = 4;
    vkb::ImageCreateInfo shadow_map_ci{
        .sType = vkb::StructureType::ImageCreateInfo,
        .imageType = vkb::ImageType::_2D,
        .format = vkb::Format::D32Sfloat,
        .extent = {shadow_resolution, shadow_resolution, 1},
        .mipLevels = 1,
        .arrayLayers = shadow_cascade_count,
        .samples = vkb::SampleCount::_1,
        .tiling = vkb::ImageTiling::Optimal,
        .usage = vkb::ImageUsage::DepthStencilAttachment | vkb::ImageUsage::Sampled,
        .sharingMode = vkb::SharingMode::Exclusive,
        .initialLayout = vkb::ImageLayout::Undefined,
    };
    vkb::Image shadow_map;
    VULL_ENSURE(context.vkCreateImage(&shadow_map_ci, &shadow_map) == vkb::Result::Success);

    vkb::MemoryRequirements shadow_map_requirements{};
    context.vkGetImageMemoryRequirements(shadow_map, &shadow_map_requirements);
    vkb::DeviceMemory shadow_map_memory = context.allocate_memory(shadow_map_requirements, MemoryType::DeviceLocal);
    VULL_ENSURE(context.vkBindImageMemory(shadow_map, shadow_map_memory, 0) == vkb::Result::Success);

    vkb::ImageViewCreateInfo shadow_map_view_ci{
        .sType = vkb::StructureType::ImageViewCreateInfo,
        .image = shadow_map,
        .viewType = vkb::ImageViewType::_2DArray,
        .format = shadow_map_ci.format,
        .subresourceRange{
            .aspectMask = vkb::ImageAspect::Depth,
            .levelCount = 1,
            .layerCount = shadow_cascade_count,
        },
    };
    vkb::ImageView shadow_map_view;
    VULL_ENSURE(context.vkCreateImageView(&shadow_map_view_ci, &shadow_map_view) == vkb::Result::Success);

    Vector<vkb::ImageView> shadow_cascade_views(shadow_cascade_count);
    for (uint32_t i = 0; i < shadow_cascade_count; i++) {
        vkb::ImageViewCreateInfo view_ci{
            .sType = vkb::StructureType::ImageViewCreateInfo,
            .image = shadow_map,
            .viewType = vkb::ImageViewType::_2DArray,
            .format = shadow_map_ci.format,
            .subresourceRange{
                .aspectMask = vkb::ImageAspect::Depth,
                .levelCount = 1,
                .baseArrayLayer = i,
                .layerCount = 1,
            },
        };
        VULL_ENSURE(context.vkCreateImageView(&view_ci, &shadow_cascade_views[i]) == vkb::Result::Success);
    }

    vkb::SamplerCreateInfo depth_sampler_ci{
        .sType = vkb::StructureType::SamplerCreateInfo,
        .magFilter = vkb::Filter::Nearest,
        .minFilter = vkb::Filter::Nearest,
        .mipmapMode = vkb::SamplerMipmapMode::Nearest,
        .addressModeU = vkb::SamplerAddressMode::ClampToEdge,
        .addressModeV = vkb::SamplerAddressMode::ClampToEdge,
        .addressModeW = vkb::SamplerAddressMode::ClampToEdge,
        .borderColor = vkb::BorderColor::FloatOpaqueWhite,
    };
    vkb::Sampler depth_sampler;
    VULL_ENSURE(context.vkCreateSampler(&depth_sampler_ci, &depth_sampler) == vkb::Result::Success);

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
    vkb::Sampler shadow_sampler;
    VULL_ENSURE(context.vkCreateSampler(&shadow_sampler_ci, &shadow_sampler) == vkb::Result::Success);

    vkb::SamplerCreateInfo albedo_sampler_ci{
        .sType = vkb::StructureType::SamplerCreateInfo,
        // TODO: Switch back to linear filtering; create a separate sampler for things wanting nearest filtering (error
        //       texture).
        .magFilter = vkb::Filter::Nearest,
        .minFilter = vkb::Filter::Nearest,
        .mipmapMode = vkb::SamplerMipmapMode::Linear,
        .addressModeU = vkb::SamplerAddressMode::Repeat,
        .addressModeV = vkb::SamplerAddressMode::Repeat,
        .addressModeW = vkb::SamplerAddressMode::Repeat,
        .anisotropyEnable = true,
        .maxAnisotropy = 16.0f,
        .maxLod = vkb::k_lod_clamp_none,
        .borderColor = vkb::BorderColor::FloatTransparentBlack,
    };
    vkb::Sampler albedo_sampler;
    VULL_ENSURE(context.vkCreateSampler(&albedo_sampler_ci, &albedo_sampler) == vkb::Result::Success);

    vkb::SamplerCreateInfo normal_sampler_ci{
        .sType = vkb::StructureType::SamplerCreateInfo,
        .magFilter = vkb::Filter::Linear,
        .minFilter = vkb::Filter::Linear,
        .mipmapMode = vkb::SamplerMipmapMode::Linear,
        .addressModeU = vkb::SamplerAddressMode::Repeat,
        .addressModeV = vkb::SamplerAddressMode::Repeat,
        .addressModeW = vkb::SamplerAddressMode::Repeat,
        .anisotropyEnable = true,
        .maxAnisotropy = 16.0f,
        .maxLod = vkb::k_lod_clamp_none,
        .borderColor = vkb::BorderColor::FloatTransparentBlack,
    };
    vkb::Sampler normal_sampler;
    VULL_ENSURE(context.vkCreateSampler(&normal_sampler_ci, &normal_sampler) == vkb::Result::Success);

    vkb::SamplerCreateInfo deferred_sampler_ci{
        .sType = vkb::StructureType::SamplerCreateInfo,
        .magFilter = vkb::Filter::Nearest,
        .minFilter = vkb::Filter::Nearest,
        .mipmapMode = vkb::SamplerMipmapMode::Nearest,
        .addressModeU = vkb::SamplerAddressMode::ClampToEdge,
        .addressModeV = vkb::SamplerAddressMode::ClampToEdge,
        .addressModeW = vkb::SamplerAddressMode::ClampToEdge,
        .borderColor = vkb::BorderColor::FloatTransparentBlack,
    };
    vkb::Sampler deferred_sampler;
    VULL_ENSURE(context.vkCreateSampler(&deferred_sampler_ci, &deferred_sampler) == vkb::Result::Success);

    struct ShadowInfo {
        Array<Mat4f, 8> cascade_matrices;
        Array<float, 8> cascade_split_depths;
    };
    struct UniformBuffer {
        Mat4f proj;
        Mat4f view;
        Vec3f camera_position;
        ShadowInfo shadow_info;
    };
    vkb::BufferCreateInfo uniform_buffer_ci{
        .sType = vkb::StructureType::BufferCreateInfo,
        .size = sizeof(UniformBuffer),
        .usage = vkb::BufferUsage::UniformBuffer,
        .sharingMode = vkb::SharingMode::Exclusive,
    };
    vkb::Buffer uniform_buffer;
    VULL_ENSURE(context.vkCreateBuffer(&uniform_buffer_ci, &uniform_buffer) == vkb::Result::Success);

    vkb::MemoryRequirements uniform_buffer_requirements{};
    context.vkGetBufferMemoryRequirements(uniform_buffer, &uniform_buffer_requirements);
    vkb::DeviceMemory uniform_buffer_memory =
        context.allocate_memory(uniform_buffer_requirements, MemoryType::HostVisible);
    VULL_ENSURE(context.vkBindBufferMemory(uniform_buffer, uniform_buffer_memory, 0) == vkb::Result::Success);

    struct PointLight {
        Vec3f position;
        float radius{0.0f};
        Vec3f colour;
        float padding{0.0f};
    };
    vkb::DeviceSize lights_buffer_size = sizeof(PointLight) * 3000 + sizeof(float) * 4;
    vkb::DeviceSize light_visibility_size = (specialisation_data.tile_max_light_count + 1) * sizeof(uint32_t);
    vkb::DeviceSize light_visibilities_buffer_size = light_visibility_size * row_tile_count * col_tile_count;

    vkb::BufferCreateInfo lights_buffer_ci{
        .sType = vkb::StructureType::BufferCreateInfo,
        .size = lights_buffer_size,
        .usage = vkb::BufferUsage::StorageBuffer,
        .sharingMode = vkb::SharingMode::Exclusive,
    };
    vkb::Buffer lights_buffer;
    VULL_ENSURE(context.vkCreateBuffer(&lights_buffer_ci, &lights_buffer) == vkb::Result::Success);

    vkb::MemoryRequirements lights_buffer_requirements{};
    context.vkGetBufferMemoryRequirements(lights_buffer, &lights_buffer_requirements);
    vkb::DeviceMemory lights_buffer_memory =
        context.allocate_memory(lights_buffer_requirements, MemoryType::HostVisible);
    VULL_ENSURE(context.vkBindBufferMemory(lights_buffer, lights_buffer_memory, 0) == vkb::Result::Success);

    vkb::BufferCreateInfo light_visibilities_buffer_ci{
        .sType = vkb::StructureType::BufferCreateInfo,
        .size = light_visibilities_buffer_size,
        .usage = vkb::BufferUsage::StorageBuffer,
        .sharingMode = vkb::SharingMode::Exclusive,
    };
    vkb::Buffer light_visibilities_buffer;
    VULL_ENSURE(context.vkCreateBuffer(&light_visibilities_buffer_ci, &light_visibilities_buffer) ==
                vkb::Result::Success);

    vkb::MemoryRequirements light_visibilities_buffer_requirements{};
    context.vkGetBufferMemoryRequirements(light_visibilities_buffer, &light_visibilities_buffer_requirements);
    vkb::DeviceMemory light_visibilities_buffer_memory =
        context.allocate_memory(light_visibilities_buffer_requirements, MemoryType::DeviceLocal);
    VULL_ENSURE(context.vkBindBufferMemory(light_visibilities_buffer, light_visibilities_buffer_memory, 0) ==
                vkb::Result::Success);

    Array descriptor_pool_sizes{
        vkb::DescriptorPoolSize{
            .type = vkb::DescriptorType::Sampler,
            .descriptorCount = 2,
        },
        vkb::DescriptorPoolSize{
            .type = vkb::DescriptorType::SampledImage,
            .descriptorCount = scene.texture_count(),
        },
        vkb::DescriptorPoolSize{
            .type = vkb::DescriptorType::UniformBuffer,
            .descriptorCount = 1,
        },
        vkb::DescriptorPoolSize{
            .type = vkb::DescriptorType::StorageBuffer,
            .descriptorCount = 2,
        },
        vkb::DescriptorPoolSize{
            .type = vkb::DescriptorType::CombinedImageSampler,
            .descriptorCount = 4,
        },
        vkb::DescriptorPoolSize{
            .type = vkb::DescriptorType::StorageImage,
            .descriptorCount = 1,
        },
    };
    vkb::DescriptorPoolCreateInfo descriptor_pool_ci{
        .sType = vkb::StructureType::DescriptorPoolCreateInfo,
        .maxSets = 3,
        .poolSizeCount = descriptor_pool_sizes.size(),
        .pPoolSizes = descriptor_pool_sizes.data(),
    };
    vkb::DescriptorPool descriptor_pool;
    VULL_ENSURE(context.vkCreateDescriptorPool(&descriptor_pool_ci, &descriptor_pool) == vkb::Result::Success);

    vkb::DescriptorSetAllocateInfo global_set_ai{
        .sType = vkb::StructureType::DescriptorSetAllocateInfo,
        .descriptorPool = descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &global_set_layout,
    };
    vkb::DescriptorSet global_set;
    VULL_ENSURE(context.vkAllocateDescriptorSets(&global_set_ai, &global_set) == vkb::Result::Success);

    vkb::DescriptorSetAllocateInfo geometry_set_ai{
        .sType = vkb::StructureType::DescriptorSetAllocateInfo,
        .descriptorPool = descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &geometry_set_layout,
    };
    vkb::DescriptorSet geometry_set;
    VULL_ENSURE(context.vkAllocateDescriptorSets(&geometry_set_ai, &geometry_set) == vkb::Result::Success);

    vkb::DescriptorSetAllocateInfo deferred_set_ai{
        .sType = vkb::StructureType::DescriptorSetAllocateInfo,
        .descriptorPool = descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &deferred_set_layout,
    };
    vkb::DescriptorSet deferred_set;
    VULL_ENSURE(context.vkAllocateDescriptorSets(&deferred_set_ai, &deferred_set) == vkb::Result::Success);

    // Global set.
    vkb::DescriptorBufferInfo uniform_buffer_info{
        .buffer = uniform_buffer,
        .range = vkb::k_whole_size,
    };
    vkb::DescriptorBufferInfo lights_buffer_info{
        .buffer = lights_buffer,
        .range = vkb::k_whole_size,
    };
    vkb::DescriptorBufferInfo light_visibilities_buffer_info{
        .buffer = light_visibilities_buffer,
        .range = vkb::k_whole_size,
    };

    // Geometry set.
    vkb::DescriptorImageInfo albedo_sampler_info{
        .sampler = albedo_sampler,
    };
    vkb::DescriptorImageInfo normal_sampler_info{
        .sampler = normal_sampler,
    };
    Vector<vkb::DescriptorImageInfo> texture_image_infos;
    texture_image_infos.ensure_capacity(scene.texture_count());
    for (auto *image_view : scene.texture_views()) {
        texture_image_infos.push(vkb::DescriptorImageInfo{
            .imageView = image_view,
            .imageLayout = vkb::ImageLayout::ShaderReadOnlyOptimal,
        });
    }

    // Deferred set.
    vkb::DescriptorImageInfo depth_sampler_image_info{
        .sampler = depth_sampler,
        .imageView = depth_image_view,
        .imageLayout = vkb::ImageLayout::ShaderReadOnlyOptimal,
    };
    vkb::DescriptorImageInfo albedo_sampler_image_info{
        .sampler = deferred_sampler,
        .imageView = albedo_image_view,
        .imageLayout = vkb::ImageLayout::ShaderReadOnlyOptimal,
    };
    vkb::DescriptorImageInfo normal_sampler_image_info{
        .sampler = deferred_sampler,
        .imageView = normal_image_view,
        .imageLayout = vkb::ImageLayout::ShaderReadOnlyOptimal,
    };
    vkb::DescriptorImageInfo shadow_map_image_info{
        .sampler = shadow_sampler,
        .imageView = shadow_map_view,
        .imageLayout = vkb::ImageLayout::ShaderReadOnlyOptimal,
    };

    Array descriptor_writes{
        // Global set.
        vkb::WriteDescriptorSet{
            .sType = vkb::StructureType::WriteDescriptorSet,
            .dstSet = global_set,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = vkb::DescriptorType::UniformBuffer,
            .pBufferInfo = &uniform_buffer_info,
        },
        vkb::WriteDescriptorSet{
            .sType = vkb::StructureType::WriteDescriptorSet,
            .dstSet = global_set,
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = vkb::DescriptorType::StorageBuffer,
            .pBufferInfo = &lights_buffer_info,
        },
        vkb::WriteDescriptorSet{
            .sType = vkb::StructureType::WriteDescriptorSet,
            .dstSet = global_set,
            .dstBinding = 2,
            .descriptorCount = 1,
            .descriptorType = vkb::DescriptorType::StorageBuffer,
            .pBufferInfo = &light_visibilities_buffer_info,
        },

        // Geometry set.
        vkb::WriteDescriptorSet{
            .sType = vkb::StructureType::WriteDescriptorSet,
            .dstSet = geometry_set,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = vkb::DescriptorType::Sampler,
            .pImageInfo = &albedo_sampler_info,
        },
        vkb::WriteDescriptorSet{
            .sType = vkb::StructureType::WriteDescriptorSet,
            .dstSet = geometry_set,
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = vkb::DescriptorType::Sampler,
            .pImageInfo = &normal_sampler_info,
        },
        vkb::WriteDescriptorSet{
            .sType = vkb::StructureType::WriteDescriptorSet,
            .dstSet = geometry_set,
            .dstBinding = 2,
            .descriptorCount = texture_image_infos.size(),
            .descriptorType = vkb::DescriptorType::SampledImage,
            .pImageInfo = texture_image_infos.data(),
        },

        // Deferred set.
        vkb::WriteDescriptorSet{
            .sType = vkb::StructureType::WriteDescriptorSet,
            .dstSet = deferred_set,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = vkb::DescriptorType::CombinedImageSampler,
            .pImageInfo = &depth_sampler_image_info,
        },
        vkb::WriteDescriptorSet{
            .sType = vkb::StructureType::WriteDescriptorSet,
            .dstSet = deferred_set,
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = vkb::DescriptorType::CombinedImageSampler,
            .pImageInfo = &albedo_sampler_image_info,
        },
        vkb::WriteDescriptorSet{
            .sType = vkb::StructureType::WriteDescriptorSet,
            .dstSet = deferred_set,
            .dstBinding = 2,
            .descriptorCount = 1,
            .descriptorType = vkb::DescriptorType::CombinedImageSampler,
            .pImageInfo = &normal_sampler_image_info,
        },
        vkb::WriteDescriptorSet{
            .sType = vkb::StructureType::WriteDescriptorSet,
            .dstSet = deferred_set,
            .dstBinding = 3,
            .descriptorCount = 1,
            .descriptorType = vkb::DescriptorType::CombinedImageSampler,
            .pImageInfo = &shadow_map_image_info,
        },
    };
    context.vkUpdateDescriptorSets(descriptor_writes.size(), descriptor_writes.data(), 0, nullptr);

    vkb::FenceCreateInfo fence_ci{
        .sType = vkb::StructureType::FenceCreateInfo,
        .flags = vkb::FenceCreateFlags::Signaled,
    };
    vkb::Fence fence;
    VULL_ENSURE(context.vkCreateFence(&fence_ci, &fence) == vkb::Result::Success);

    vkb::SemaphoreCreateInfo semaphore_ci{
        .sType = vkb::StructureType::SemaphoreCreateInfo,
    };
    vkb::Semaphore image_available_semaphore;
    vkb::Semaphore rendering_finished_semaphore;
    VULL_ENSURE(context.vkCreateSemaphore(&semaphore_ci, &image_available_semaphore) == vkb::Result::Success);
    VULL_ENSURE(context.vkCreateSemaphore(&semaphore_ci, &rendering_finished_semaphore) == vkb::Result::Success);

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
        const float shadow_distance = 2000.0f;
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

            ubo.shadow_info.cascade_matrices[i] = proj * view;
            ubo.shadow_info.cascade_split_depths[i] = (near_plane + split_distances[i] * clip_range);
            last_split_distance = split_distances[i];
        }
    };

    void *lights_data;
    void *ubo_data;
    context.vkMapMemory(lights_buffer_memory, 0, vkb::k_whole_size, 0, &lights_data);
    context.vkMapMemory(uniform_buffer_memory, 0, vkb::k_whole_size, 0, &ubo_data);

    vkb::QueryPoolCreateInfo query_pool_ci{
        .sType = vkb::StructureType::QueryPoolCreateInfo,
        .queryType = vkb::QueryType::Timestamp,
        .queryCount = 6,
    };
    vkb::QueryPool query_pool;
    context.vkCreateQueryPool(&query_pool_ci, &query_pool);

    ui::Renderer ui(context, swapchain, ui_vertex_shader, ui_fragment_shader);
    ui::TimeGraph cpu_time_graph(Vec2f(600.0f, 300.0f), Vec3f(0.6f, 0.7f, 0.8f));
    ui::TimeGraph gpu_time_graph(Vec2f(600.0f, 300.0f), Vec3f(0.8f, 0.0f, 0.7f));
    auto font = ui.load_font("../engine/fonts/DejaVuSansMono.ttf", 20);
    ui.set_global_scale(window.ppcm() / 37.8f * 0.55f);

    vkb::PhysicalDeviceProperties device_properties{};
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

        Array<uint64_t, 6> timestamp_data{};
        context.vkGetQueryPoolResults(query_pool, 0, timestamp_data.size(), timestamp_data.size_bytes(),
                                      timestamp_data.data(), sizeof(uint64_t), vkb::QueryResultFlags::_64);

        ui::TimeGraph::Bar gpu_frame_bar;
        gpu_frame_bar.sections.push({"Geometry pass", (static_cast<float>((timestamp_data[1] - timestamp_data[0])) *
                                                       device_properties.limits.timestampPeriod) /
                                                          1000000000.0f});
        gpu_frame_bar.sections.push({"Shadow pass", (static_cast<float>((timestamp_data[2] - timestamp_data[1])) *
                                                     device_properties.limits.timestampPeriod) /
                                                        1000000000.0f});
        gpu_frame_bar.sections.push({"Light cull", (static_cast<float>((timestamp_data[3] - timestamp_data[2])) *
                                                    device_properties.limits.timestampPeriod) /
                                                       1000000000.0f});
        gpu_frame_bar.sections.push({"Deferred pass", (static_cast<float>((timestamp_data[4] - timestamp_data[3])) *
                                                       device_properties.limits.timestampPeriod) /
                                                          1000000000.0f});
        gpu_frame_bar.sections.push({"UI", (static_cast<float>((timestamp_data[5] - timestamp_data[4])) *
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

        vkb::DescriptorImageInfo output_image_info{
            .imageView = swapchain.image_view(image_index),
            .imageLayout = vkb::ImageLayout::General,
        };
        vkb::WriteDescriptorSet output_image_write{
            .sType = vkb::StructureType::WriteDescriptorSet,
            .dstSet = global_set,
            .dstBinding = 3,
            .descriptorCount = 1,
            .descriptorType = vkb::DescriptorType::StorageImage,
            .pImageInfo = &output_image_info,
        };
        context.vkUpdateDescriptorSets(1, &output_image_write, 0, nullptr);

        Timer record_timer;
        const auto &cmd_buf = cmd_pool.request_cmd_buf();
        cmd_buf.reset_query_pool(query_pool, query_pool_ci.queryCount);

        Array compute_sets{global_set, deferred_set};
        cmd_buf.bind_descriptor_sets(vkb::PipelineBindPoint::Compute, compute_pipeline_layout, compute_sets.span());

        Array graphics_sets{global_set, geometry_set};
        cmd_buf.bind_descriptor_sets(vkb::PipelineBindPoint::Graphics, geometry_pipeline_layout, graphics_sets.span());

        Array gbuffer_write_barriers{
            vkb::ImageMemoryBarrier{
                .sType = vkb::StructureType::ImageMemoryBarrier,
                .dstAccessMask = vkb::Access::ColorAttachmentWrite,
                .oldLayout = vkb::ImageLayout::Undefined,
                .newLayout = vkb::ImageLayout::ColorAttachmentOptimal,
                .image = albedo_image,
                .subresourceRange{
                    .aspectMask = vkb::ImageAspect::Color,
                    .levelCount = 1,
                    .layerCount = 1,
                },
            },
            vkb::ImageMemoryBarrier{
                .sType = vkb::StructureType::ImageMemoryBarrier,
                .dstAccessMask = vkb::Access::ColorAttachmentWrite,
                .oldLayout = vkb::ImageLayout::Undefined,
                .newLayout = vkb::ImageLayout::ColorAttachmentOptimal,
                .image = normal_image,
                .subresourceRange{
                    .aspectMask = vkb::ImageAspect::Color,
                    .levelCount = 1,
                    .layerCount = 1,
                },
            },
        };
        cmd_buf.pipeline_barrier(vkb::PipelineStage::TopOfPipe, vkb::PipelineStage::ColorAttachmentOutput, {},
                                 gbuffer_write_barriers.span());

        vkb::ImageMemoryBarrier depth_write_barrier{
            .sType = vkb::StructureType::ImageMemoryBarrier,
            .dstAccessMask = vkb::Access::DepthStencilAttachmentWrite,
            .oldLayout = vkb::ImageLayout::Undefined,
            .newLayout = vkb::ImageLayout::DepthAttachmentOptimal,
            .image = depth_image,
            .subresourceRange{
                .aspectMask = vkb::ImageAspect::Depth,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        cmd_buf.pipeline_barrier(vkb::PipelineStage::TopOfPipe,
                                 vkb::PipelineStage::EarlyFragmentTests | vkb::PipelineStage::LateFragmentTests, {},
                                 depth_write_barrier);

        Array gbuffer_write_attachments{
            vkb::RenderingAttachmentInfo{
                .sType = vkb::StructureType::RenderingAttachmentInfo,
                .imageView = albedo_image_view,
                .imageLayout = vkb::ImageLayout::ColorAttachmentOptimal,
                .loadOp = vkb::AttachmentLoadOp::Clear,
                .storeOp = vkb::AttachmentStoreOp::Store,
                .clearValue{
                    .color{{0.0f, 0.0f, 0.0f, 0.0f}},
                },
            },
            vkb::RenderingAttachmentInfo{
                .sType = vkb::StructureType::RenderingAttachmentInfo,
                .imageView = normal_image_view,
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
            .imageView = depth_image_view,
            .imageLayout = vkb::ImageLayout::DepthAttachmentOptimal,
            .loadOp = vkb::AttachmentLoadOp::Clear,
            .storeOp = vkb::AttachmentStoreOp::Store,
            .clearValue{
                .depthStencil{0.0f, 0},
            },
        };
        vkb::RenderingInfo geometry_pass_rendering_info{
            .sType = vkb::StructureType::RenderingInfo,
            .renderArea{
                .extent = swapchain.extent_2D(),
            },
            .layerCount = 1,
            .colorAttachmentCount = gbuffer_write_attachments.size(),
            .pColorAttachments = gbuffer_write_attachments.data(),
            .pDepthAttachment = &depth_write_attachment,
            .pStencilAttachment = &depth_write_attachment,
        };
        cmd_buf.write_timestamp(vkb::PipelineStage::TopOfPipe, query_pool, 0);
        cmd_buf.begin_rendering(geometry_pass_rendering_info);
        cmd_buf.bind_pipeline(vkb::PipelineBindPoint::Graphics, geometry_pass_pipeline);
        scene.render(cmd_buf, geometry_pipeline_layout, 0);
        cmd_buf.end_rendering();

        vkb::ImageMemoryBarrier shadow_map_write_barrier{
            .sType = vkb::StructureType::ImageMemoryBarrier,
            .dstAccessMask = vkb::Access::DepthStencilAttachmentWrite,
            .oldLayout = vkb::ImageLayout::Undefined,
            .newLayout = vkb::ImageLayout::DepthAttachmentOptimal,
            .image = shadow_map,
            .subresourceRange{
                .aspectMask = vkb::ImageAspect::Depth,
                .levelCount = 1,
                .layerCount = shadow_cascade_count,
            },
        };
        cmd_buf.pipeline_barrier(vkb::PipelineStage::TopOfPipe,
                                 vkb::PipelineStage::EarlyFragmentTests | vkb::PipelineStage::LateFragmentTests, {},
                                 shadow_map_write_barrier);
        cmd_buf.write_timestamp(vkb::PipelineStage::AllGraphics, query_pool, 1);

        cmd_buf.bind_pipeline(vkb::PipelineBindPoint::Graphics, shadow_pass_pipeline);
        for (uint32_t i = 0; i < shadow_cascade_count; i++) {
            vkb::RenderingAttachmentInfo shadow_map_write_attachment{
                .sType = vkb::StructureType::RenderingAttachmentInfo,
                .imageView = shadow_cascade_views[i],
                .imageLayout = vkb::ImageLayout::DepthAttachmentOptimal,
                .loadOp = vkb::AttachmentLoadOp::Clear,
                .storeOp = vkb::AttachmentStoreOp::Store,
                .clearValue{
                    .depthStencil{1.0f, 0},
                },
            };
            vkb::RenderingInfo shadow_map_rendering_info{
                .sType = vkb::StructureType::RenderingInfo,
                .renderArea{
                    .extent = {shadow_resolution, shadow_resolution},
                },
                .layerCount = 1,
                .pDepthAttachment = &shadow_map_write_attachment,
                .pStencilAttachment = &shadow_map_write_attachment,
            };
            cmd_buf.begin_rendering(shadow_map_rendering_info);
            scene.render(cmd_buf, geometry_pipeline_layout, i);
            cmd_buf.end_rendering();
        }

        vkb::ImageMemoryBarrier depth_sample_barrier{
            .sType = vkb::StructureType::ImageMemoryBarrier,
            .srcAccessMask = vkb::Access::DepthStencilAttachmentWrite,
            .dstAccessMask = vkb::Access::ShaderRead,
            .oldLayout = vkb::ImageLayout::DepthAttachmentOptimal,
            .newLayout = vkb::ImageLayout::ShaderReadOnlyOptimal,
            .image = depth_image,
            .subresourceRange{
                .aspectMask = vkb::ImageAspect::Depth,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        cmd_buf.pipeline_barrier(vkb::PipelineStage::EarlyFragmentTests | vkb::PipelineStage::LateFragmentTests,
                                 vkb::PipelineStage::ComputeShader, {}, depth_sample_barrier);
        cmd_buf.write_timestamp(vkb::PipelineStage::AllGraphics, query_pool, 2);
        cmd_buf.bind_pipeline(vkb::PipelineBindPoint::Compute, light_cull_pipeline);
        cmd_buf.dispatch(row_tile_count, col_tile_count, 1);

        Array deferred_pass_buffer_barriers{
            vkb::BufferMemoryBarrier{
                .sType = vkb::StructureType::BufferMemoryBarrier,
                .srcAccessMask = vkb::Access::ShaderWrite,
                .dstAccessMask = vkb::Access::ShaderRead,
                .buffer = lights_buffer,
                .size = lights_buffer_size,
            },
            vkb::BufferMemoryBarrier{
                .sType = vkb::StructureType::BufferMemoryBarrier,
                .srcAccessMask = vkb::Access::ShaderWrite,
                .dstAccessMask = vkb::Access::ShaderRead,
                .buffer = light_visibilities_buffer,
                .size = light_visibilities_buffer_size,
            },
        };
        cmd_buf.pipeline_barrier(vkb::PipelineStage::ComputeShader, vkb::PipelineStage::ComputeShader,
                                 deferred_pass_buffer_barriers.span(), {});
        cmd_buf.write_timestamp(vkb::PipelineStage::ComputeShader, query_pool, 3);

        Array gbuffer_sample_barriers{
            vkb::ImageMemoryBarrier{
                .sType = vkb::StructureType::ImageMemoryBarrier,
                .srcAccessMask = vkb::Access::ColorAttachmentWrite,
                .dstAccessMask = vkb::Access::ShaderRead,
                .oldLayout = vkb::ImageLayout::ColorAttachmentOptimal,
                .newLayout = vkb::ImageLayout::ShaderReadOnlyOptimal,
                .image = albedo_image,
                .subresourceRange{
                    .aspectMask = vkb::ImageAspect::Color,
                    .levelCount = 1,
                    .layerCount = 1,
                },
            },
            vkb::ImageMemoryBarrier{
                .sType = vkb::StructureType::ImageMemoryBarrier,
                .srcAccessMask = vkb::Access::ColorAttachmentWrite,
                .dstAccessMask = vkb::Access::ShaderRead,
                .oldLayout = vkb::ImageLayout::ColorAttachmentOptimal,
                .newLayout = vkb::ImageLayout::ShaderReadOnlyOptimal,
                .image = normal_image,
                .subresourceRange{
                    .aspectMask = vkb::ImageAspect::Color,
                    .levelCount = 1,
                    .layerCount = 1,
                },
            },
        };
        cmd_buf.pipeline_barrier(vkb::PipelineStage::ColorAttachmentOutput, vkb::PipelineStage::ComputeShader, {},
                                 gbuffer_sample_barriers.span());

        vkb::ImageMemoryBarrier output_image_barrier{
            .sType = vkb::StructureType::ImageMemoryBarrier,
            .dstAccessMask = vkb::Access::ShaderWrite,
            .oldLayout = vkb::ImageLayout::Undefined,
            .newLayout = vkb::ImageLayout::General,
            .image = swapchain.image(image_index),
            .subresourceRange{
                .aspectMask = vkb::ImageAspect::Color,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        cmd_buf.pipeline_barrier(vkb::PipelineStage::TopOfPipe, vkb::PipelineStage::ComputeShader, {},
                                 output_image_barrier);

        vkb::ImageMemoryBarrier shadow_map_sample_barrier{
            .sType = vkb::StructureType::ImageMemoryBarrier,
            .srcAccessMask = vkb::Access::DepthStencilAttachmentWrite,
            .dstAccessMask = vkb::Access::ShaderRead,
            .oldLayout = vkb::ImageLayout::DepthAttachmentOptimal,
            .newLayout = vkb::ImageLayout::ShaderReadOnlyOptimal,
            .image = shadow_map,
            .subresourceRange{
                .aspectMask = vkb::ImageAspect::Depth,
                .levelCount = 1,
                .layerCount = shadow_cascade_count,
            },
        };
        cmd_buf.pipeline_barrier(vkb::PipelineStage::EarlyFragmentTests | vkb::PipelineStage::LateFragmentTests,
                                 vkb::PipelineStage::ComputeShader, {}, shadow_map_sample_barrier);

        cmd_buf.bind_pipeline(vkb::PipelineBindPoint::Compute, deferred_pipeline);
        cmd_buf.dispatch(window.width() / 8, window.height() / 8, 1);

        vkb::ImageMemoryBarrier ui_colour_write_barrier{
            .sType = vkb::StructureType::ImageMemoryBarrier,
            .srcAccessMask = vkb::Access::ShaderWrite,
            .dstAccessMask = vkb::Access::ColorAttachmentRead,
            .oldLayout = vkb::ImageLayout::General,
            .newLayout = vkb::ImageLayout::ColorAttachmentOptimal,
            .image = swapchain.image(image_index),
            .subresourceRange{
                .aspectMask = vkb::ImageAspect::Color,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        cmd_buf.pipeline_barrier(vkb::PipelineStage::ComputeShader, vkb::PipelineStage::ColorAttachmentOutput, {},
                                 ui_colour_write_barrier);

        cmd_buf.write_timestamp(vkb::PipelineStage::ComputeShader, query_pool, 4);
        ui.render(cmd_buf, image_index);
        cmd_buf.write_timestamp(vkb::PipelineStage::AllGraphics, query_pool, 5);

        vkb::ImageMemoryBarrier colour_present_barrier{
            .sType = vkb::StructureType::ImageMemoryBarrier,
            .srcAccessMask = vkb::Access::ColorAttachmentWrite,
            .oldLayout = vkb::ImageLayout::ColorAttachmentOptimal,
            .newLayout = vkb::ImageLayout::PresentSrcKHR,
            .image = swapchain.image(image_index),
            .subresourceRange{
                .aspectMask = vkb::ImageAspect::Color,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        cmd_buf.pipeline_barrier(vkb::PipelineStage::ColorAttachmentOutput, vkb::PipelineStage::BottomOfPipe, {},
                                 colour_present_barrier);

        Array signal_semaphores{
            vkb::SemaphoreSubmitInfo{
                .sType = vkb::StructureType::SemaphoreSubmitInfo,
                .semaphore = rendering_finished_semaphore,
            },
        };
        Array wait_semaphores{
            vkb::SemaphoreSubmitInfo{
                .sType = vkb::StructureType::SemaphoreSubmitInfo,
                .semaphore = image_available_semaphore,
                .stageMask = static_cast<vkb::PipelineStageFlags2>(vkb::PipelineStage::ColorAttachmentOutput),
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
    context.vkDestroySampler(deferred_sampler);
    context.vkDestroySampler(normal_sampler);
    context.vkDestroySampler(albedo_sampler);
    context.vkDestroySampler(shadow_sampler);
    context.vkDestroySampler(depth_sampler);
    for (auto *cascade_view : shadow_cascade_views) {
        context.vkDestroyImageView(cascade_view);
    }
    context.vkDestroyImageView(shadow_map_view);
    context.vkFreeMemory(shadow_map_memory);
    context.vkDestroyImage(shadow_map);
    context.vkDestroyImageView(normal_image_view);
    context.vkFreeMemory(normal_image_memory);
    context.vkDestroyImage(normal_image);
    context.vkDestroyImageView(albedo_image_view);
    context.vkFreeMemory(albedo_image_memory);
    context.vkDestroyImage(albedo_image);
    context.vkDestroyImageView(depth_image_view);
    context.vkFreeMemory(depth_image_memory);
    context.vkDestroyImage(depth_image);
    context.vkDestroyPipeline(deferred_pipeline);
    context.vkDestroyPipeline(light_cull_pipeline);
    context.vkDestroyPipeline(shadow_pass_pipeline);
    context.vkDestroyPipeline(geometry_pass_pipeline);
    context.vkDestroyPipelineLayout(compute_pipeline_layout);
    context.vkDestroyPipelineLayout(geometry_pipeline_layout);
    context.vkDestroyDescriptorSetLayout(deferred_set_layout);
    context.vkDestroyDescriptorSetLayout(geometry_set_layout);
    context.vkDestroyDescriptorSetLayout(global_set_layout);
    context.vkDestroyShaderModule(ui_fragment_shader);
    context.vkDestroyShaderModule(ui_vertex_shader);
    context.vkDestroyShaderModule(shadow_shader);
    context.vkDestroyShaderModule(light_cull_shader);
    context.vkDestroyShaderModule(deferred_shader);
    context.vkDestroyShaderModule(default_fragment_shader);
    context.vkDestroyShaderModule(default_vertex_shader);
}

} // namespace

int main() {
    Scheduler scheduler;
    scheduler.start([&] {
        main_task(scheduler);
    });
}
