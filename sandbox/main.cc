#include "FreeCamera.hh"
#include "OrbitCamera.hh"

#include <vull/core/Mesh.hh>
#include <vull/core/Scene.hh>
#include <vull/core/Vertex.hh>
#include <vull/core/Window.hh>
#include <vull/ecs/Entity.hh>
#include <vull/ecs/EntityId.hh>
#include <vull/ecs/World.hh>
#include <vull/maths/Common.hh>
#include <vull/maths/Mat.hh>
#include <vull/maths/Quat.hh>
#include <vull/maths/Random.hh>
#include <vull/maths/Vec.hh>
#include <vull/physics/Collider.hh>
#include <vull/physics/PhysicsEngine.hh>
#include <vull/physics/RigidBody.hh>
#include <vull/physics/Shape.hh>
#include <vull/support/Array.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Format.hh>
#include <vull/support/Span.hh>
#include <vull/support/String.hh>
#include <vull/support/StringView.hh>
#include <vull/support/Timer.hh>
#include <vull/support/Tuple.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/support/Vector.hh>
#include <vull/tasklet/Scheduler.hh>
#include <vull/tasklet/Tasklet.hh> // IWYU pragma: keep
#include <vull/ui/Renderer.hh>
#include <vull/ui/TimeGraph.hh>
#include <vull/vulkan/CommandBuffer.hh>
#include <vull/vulkan/CommandPool.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/QueryPool.hh>
#include <vull/vulkan/Queue.hh>
#include <vull/vulkan/RenderGraph.hh>
#include <vull/vulkan/Swapchain.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <vull/core/Material.hh>
#include <vull/core/Transform.hh>

using namespace vull;

namespace {

uint32_t find_graphics_family(const vk::Context &context) {
    for (uint32_t i = 0; i < context.queue_families().size(); i++) {
        const auto &family = context.queue_families()[i];
        if ((family.queueFlags & vkb::QueueFlags::Graphics) != vkb::QueueFlags::None) {
            return i;
        }
    }
    VULL_ENSURE_NOT_REACHED();
}

vkb::ShaderModule load_shader(const vk::Context &context, const char *path) {
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
    vk::Context context;
    auto swapchain = window.create_swapchain(context, vk::SwapchainMode::LowPower);

    const auto graphics_family_index = find_graphics_family(context);
    vk::CommandPool cmd_pool(context, graphics_family_index);
    vk::Queue queue(context, graphics_family_index);

    Scene scene(context);
    scene.load(cmd_pool, queue, "scene.vpak");

    constexpr uint32_t tile_size = 32;
    uint32_t row_tile_count = vull::ceil_div(window.width(), tile_size);
    uint32_t col_tile_count = vull::ceil_div(window.height(), tile_size);

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
    Array frame_set_bindings{
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
    vkb::DescriptorSetLayoutCreateInfo frame_set_layout_ci{
        .sType = vkb::StructureType::DescriptorSetLayoutCreateInfo,
        .bindingCount = frame_set_bindings.size(),
        .pBindings = frame_set_bindings.data(),
    };
    vkb::DescriptorSetLayout frame_set_layout;
    VULL_ENSURE(context.vkCreateDescriptorSetLayout(&frame_set_layout_ci, &frame_set_layout) == vkb::Result::Success);

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
        frame_set_layout,
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
        frame_set_layout,
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
    vkb::DeviceMemory depth_image_memory =
        context.allocate_memory(depth_image_requirements, vk::MemoryType::DeviceLocal);
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
    vkb::DeviceMemory albedo_image_memory =
        context.allocate_memory(albedo_image_requirements, vk::MemoryType::DeviceLocal);
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
    vkb::DeviceMemory normal_image_memory =
        context.allocate_memory(normal_image_requirements, vk::MemoryType::DeviceLocal);
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
    vkb::DeviceMemory shadow_map_memory = context.allocate_memory(shadow_map_requirements, vk::MemoryType::DeviceLocal);
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
    Array<Tuple<vkb::Buffer, vkb::DeviceMemory>, 2> uniform_buffers;
    for (auto &[buffer, memory] : uniform_buffers) {
        VULL_ENSURE(context.vkCreateBuffer(&uniform_buffer_ci, &buffer) == vkb::Result::Success);
        vkb::MemoryRequirements memory_requirements{};
        context.vkGetBufferMemoryRequirements(buffer, &memory_requirements);
        memory = context.allocate_memory(memory_requirements, vk::MemoryType::HostVisible);
        VULL_ENSURE(context.vkBindBufferMemory(buffer, memory, 0) == vkb::Result::Success);
    }

    struct PointLight {
        Vec3f position;
        float radius{0.0f};
        Vec3f colour;
        float padding{0.0f};
    };
    vkb::DeviceSize lights_buffer_size = sizeof(PointLight) * 3000 + sizeof(float) * 4;
    vkb::DeviceSize light_visibility_size = (specialisation_data.tile_max_light_count + 1) * sizeof(uint32_t);
    vkb::DeviceSize light_visibilities_buffer_size = light_visibility_size * row_tile_count * col_tile_count;

    vkb::BufferCreateInfo light_buffer_ci{
        .sType = vkb::StructureType::BufferCreateInfo,
        .size = lights_buffer_size,
        .usage = vkb::BufferUsage::StorageBuffer,
        .sharingMode = vkb::SharingMode::Exclusive,
    };
    Array<Tuple<vkb::Buffer, vkb::DeviceMemory>, 2> light_buffers;
    for (auto &[buffer, memory] : light_buffers) {
        VULL_ENSURE(context.vkCreateBuffer(&light_buffer_ci, &buffer) == vkb::Result::Success);
        vkb::MemoryRequirements memory_requirements{};
        context.vkGetBufferMemoryRequirements(buffer, &memory_requirements);
        memory = context.allocate_memory(memory_requirements, vk::MemoryType::HostVisible);
        VULL_ENSURE(context.vkBindBufferMemory(buffer, memory, 0) == vkb::Result::Success);
    }

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
        context.allocate_memory(light_visibilities_buffer_requirements, vk::MemoryType::DeviceLocal);
    VULL_ENSURE(context.vkBindBufferMemory(light_visibilities_buffer, light_visibilities_buffer_memory, 0) ==
                vkb::Result::Success);

    Array descriptor_pool_sizes{
        vkb::DescriptorPoolSize{
            .type = vkb::DescriptorType::Sampler,
            .descriptorCount = 2,
        },
        vkb::DescriptorPoolSize{
            .type = vkb::DescriptorType::SampledImage,
            .descriptorCount = scene.texture_count() + 3,
        },
        vkb::DescriptorPoolSize{
            .type = vkb::DescriptorType::UniformBuffer,
            .descriptorCount = 2,
        },
        vkb::DescriptorPoolSize{
            .type = vkb::DescriptorType::StorageBuffer,
            .descriptorCount = 4,
        },
        vkb::DescriptorPoolSize{
            .type = vkb::DescriptorType::CombinedImageSampler,
            .descriptorCount = 1,
        },
        vkb::DescriptorPoolSize{
            .type = vkb::DescriptorType::StorageImage,
            .descriptorCount = 2,
        },
    };
    vkb::DescriptorPoolCreateInfo descriptor_pool_ci{
        .sType = vkb::StructureType::DescriptorPoolCreateInfo,
        .maxSets = 4,
        .poolSizeCount = descriptor_pool_sizes.size(),
        .pPoolSizes = descriptor_pool_sizes.data(),
    };
    vkb::DescriptorPool descriptor_pool;
    VULL_ENSURE(context.vkCreateDescriptorPool(&descriptor_pool_ci, &descriptor_pool) == vkb::Result::Success);

    Array<vkb::DescriptorSet, 2> frame_sets;
    Array frame_set_layouts{frame_set_layout, frame_set_layout};
    vkb::DescriptorSetAllocateInfo frame_set_ai{
        .sType = vkb::StructureType::DescriptorSetAllocateInfo,
        .descriptorPool = descriptor_pool,
        .descriptorSetCount = frame_sets.size(),
        .pSetLayouts = frame_set_layouts.data(),
    };
    VULL_ENSURE(context.vkAllocateDescriptorSets(&frame_set_ai, frame_sets.data()) == vkb::Result::Success);

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

    // Frame set.
    Array<vkb::DescriptorBufferInfo, 2> uniform_buffer_infos{
        vkb::DescriptorBufferInfo{
            .buffer = vull::get<0>(uniform_buffers[0]),
            .range = vkb::k_whole_size,
        },
        vkb::DescriptorBufferInfo{
            .buffer = vull::get<0>(uniform_buffers[1]),
            .range = vkb::k_whole_size,
        },
    };
    Array<vkb::DescriptorBufferInfo, 2> light_buffer_infos{
        vkb::DescriptorBufferInfo{
            .buffer = vull::get<0>(light_buffers[0]),
            .range = vkb::k_whole_size,
        },
        vkb::DescriptorBufferInfo{
            .buffer = vull::get<0>(light_buffers[1]),
            .range = vkb::k_whole_size,
        },
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
            .imageLayout = vkb::ImageLayout::ReadOnlyOptimal,
        });
    }

    // Deferred set.
    vkb::DescriptorImageInfo depth_image_info{
        .imageView = depth_image_view,
        .imageLayout = vkb::ImageLayout::ReadOnlyOptimal,
    };
    vkb::DescriptorImageInfo albedo_image_info{
        .imageView = albedo_image_view,
        .imageLayout = vkb::ImageLayout::ReadOnlyOptimal,
    };
    vkb::DescriptorImageInfo normal_image_info{
        .imageView = normal_image_view,
        .imageLayout = vkb::ImageLayout::ReadOnlyOptimal,
    };
    vkb::DescriptorImageInfo shadow_map_image_info{
        .sampler = shadow_sampler,
        .imageView = shadow_map_view,
        .imageLayout = vkb::ImageLayout::ReadOnlyOptimal,
    };

    Array descriptor_writes{
        // First frame set.
        vkb::WriteDescriptorSet{
            .sType = vkb::StructureType::WriteDescriptorSet,
            .dstSet = frame_sets[0],
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = vkb::DescriptorType::UniformBuffer,
            .pBufferInfo = &uniform_buffer_infos[0],
        },
        vkb::WriteDescriptorSet{
            .sType = vkb::StructureType::WriteDescriptorSet,
            .dstSet = frame_sets[0],
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = vkb::DescriptorType::StorageBuffer,
            .pBufferInfo = &light_buffer_infos[0],
        },
        vkb::WriteDescriptorSet{
            .sType = vkb::StructureType::WriteDescriptorSet,
            .dstSet = frame_sets[0],
            .dstBinding = 2,
            .descriptorCount = 1,
            .descriptorType = vkb::DescriptorType::StorageBuffer,
            .pBufferInfo = &light_visibilities_buffer_info,
        },

        // Second frame set.
        vkb::WriteDescriptorSet{
            .sType = vkb::StructureType::WriteDescriptorSet,
            .dstSet = frame_sets[1],
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = vkb::DescriptorType::UniformBuffer,
            .pBufferInfo = &uniform_buffer_infos[1],
        },
        vkb::WriteDescriptorSet{
            .sType = vkb::StructureType::WriteDescriptorSet,
            .dstSet = frame_sets[1],
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = vkb::DescriptorType::StorageBuffer,
            .pBufferInfo = &light_buffer_infos[1],
        },
        vkb::WriteDescriptorSet{
            .sType = vkb::StructureType::WriteDescriptorSet,
            .dstSet = frame_sets[1],
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
            .descriptorType = vkb::DescriptorType::SampledImage,
            .pImageInfo = &depth_image_info,
        },
        vkb::WriteDescriptorSet{
            .sType = vkb::StructureType::WriteDescriptorSet,
            .dstSet = deferred_set,
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = vkb::DescriptorType::SampledImage,
            .pImageInfo = &albedo_image_info,
        },
        vkb::WriteDescriptorSet{
            .sType = vkb::StructureType::WriteDescriptorSet,
            .dstSet = deferred_set,
            .dstBinding = 2,
            .descriptorCount = 1,
            .descriptorType = vkb::DescriptorType::SampledImage,
            .pImageInfo = &normal_image_info,
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

    Vector<PointLight> lights(50);
    for (auto &light : lights) {
        light.colour = vull::linear_rand(Vec3f(0.1f), Vec3f(1.0f));
        light.radius = vull::linear_rand(2.5f, 15.0f);
        light.position = vull::linear_rand(Vec3f(-50.0f, 2.0f, -70.0f), Vec3f(100.0f, 30.0f, 50.0f));
    }

    FreeCamera free_camera;
    OrbitCamera orbit_camera;
    free_camera.set_position({20.0f, 15.0f, -20.0f});
    free_camera.set_pitch(-0.3f);
    free_camera.set_yaw(2.4f);

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
            radius = vull::ceil(radius * 16.0f) / 16.0f;

            // TODO: direction duplicated in shader.
            constexpr Vec3f direction(0.6f, 0.6f, -0.6f);
            constexpr Vec3f up(0.0f, 1.0f, 0.0f);
            auto proj = vull::ortho(-radius, radius, -radius, radius, 0.0f, radius * 2.0f);
            auto view = vull::look_at(frustum_center + direction * radius, frustum_center, up);

            // Apply a small correction factor to the projection matrix to snap texels and avoid shimmering around the
            // edges of shadows.
            Vec4f origin = (proj * view * Vec4f(0.0f, 0.0f, 0.0f, 1.0f)) * (shadow_resolution / 2.0f);
            Vec2f rounded_origin(vull::round(origin.x()), vull::round(origin.y()));
            Vec2f round_offset = (rounded_origin - origin) * (2.0f / shadow_resolution);
            proj[3] += Vec4f(round_offset, 0.0f, 0.0f);

            ubo.shadow_info.cascade_matrices[i] = proj * view;
            ubo.shadow_info.cascade_split_depths[i] = (near_plane + split_distances[i] * clip_range);
            last_split_distance = split_distances[i];
        }
    };

    Array<void *, 2> light_data_ptrs;
    Array<void *, 2> ubo_data_ptrs;
    context.vkMapMemory(vull::get<1>(light_buffers[0]), 0, vkb::k_whole_size, 0, &light_data_ptrs[0]);
    context.vkMapMemory(vull::get<1>(light_buffers[1]), 0, vkb::k_whole_size, 0, &light_data_ptrs[1]);
    context.vkMapMemory(vull::get<1>(uniform_buffers[0]), 0, vkb::k_whole_size, 0, &ubo_data_ptrs[0]);
    context.vkMapMemory(vull::get<1>(uniform_buffers[1]), 0, vkb::k_whole_size, 0, &ubo_data_ptrs[1]);

    vk::RenderGraph render_graph;

    // GBuffer resources.
    auto &albedo_image_resource = render_graph.add_image("GBuffer albedo");
    auto &normal_image_resource = render_graph.add_image("GBuffer normal");
    auto &depth_image_resource = render_graph.add_image("GBuffer depth");
    albedo_image_resource.set_image(albedo_image, albedo_image_view, albedo_image_view_ci.subresourceRange);
    normal_image_resource.set_image(normal_image, normal_image_view, normal_image_view_ci.subresourceRange);
    depth_image_resource.set_image(depth_image, depth_image_view, depth_image_view_ci.subresourceRange);

    auto &shadow_map_resource = render_graph.add_image("Shadow map");
    shadow_map_resource.set_image(shadow_map, shadow_map_view, shadow_map_view_ci.subresourceRange);

    auto &swapchain_resource = render_graph.add_image("Swapchain");
    swapchain_resource.set_image(nullptr, nullptr,
                                 {
                                     .aspectMask = vkb::ImageAspect::Color,
                                     .levelCount = 1,
                                     .layerCount = 1,
                                 });

    auto &global_ubo_resource = render_graph.add_uniform_buffer("Global UBO");
    auto &light_data_resource = render_graph.add_storage_buffer("Light data");
    auto &light_visibility_data_resource = render_graph.add_storage_buffer("Light visibility data");
    light_visibility_data_resource.set_buffer(light_visibilities_buffer);

    auto &geometry_pass = render_graph.add_graphics_pass("Geometry pass");
    geometry_pass.reads_from(global_ubo_resource);
    geometry_pass.writes_to(albedo_image_resource);
    geometry_pass.writes_to(normal_image_resource);
    geometry_pass.writes_to(depth_image_resource);
    geometry_pass.set_on_record([&](const vk::CommandBuffer &cmd_buf) {
        Array colour_write_attachments{
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
        vkb::RenderingInfo rendering_info{
            .sType = vkb::StructureType::RenderingInfo,
            .renderArea{
                .extent = swapchain.extent_2D(),
            },
            .layerCount = 1,
            .colorAttachmentCount = colour_write_attachments.size(),
            .pColorAttachments = colour_write_attachments.data(),
            .pDepthAttachment = &depth_write_attachment,
        };
        cmd_buf.bind_pipeline(vkb::PipelineBindPoint::Graphics, geometry_pass_pipeline);
        cmd_buf.begin_rendering(rendering_info);
        scene.render(cmd_buf, geometry_pipeline_layout, 0);
        cmd_buf.end_rendering();
    });

    auto &shadow_pass = render_graph.add_graphics_pass("Shadow pass");
    shadow_pass.reads_from(global_ubo_resource);
    shadow_pass.writes_to(shadow_map_resource);
    shadow_pass.set_on_record([&](const vk::CommandBuffer &cmd_buf) {
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
            vkb::RenderingInfo rendering_info{
                .sType = vkb::StructureType::RenderingInfo,
                .renderArea{
                    .extent = {shadow_resolution, shadow_resolution},
                },
                .layerCount = 1,
                .pDepthAttachment = &shadow_map_write_attachment,
            };
            cmd_buf.begin_rendering(rendering_info);
            scene.render(cmd_buf, geometry_pipeline_layout, i);
            cmd_buf.end_rendering();
        }
    });

    auto &light_cull_pass = render_graph.add_compute_pass("Light cull");
    light_cull_pass.reads_from(global_ubo_resource);
    light_cull_pass.reads_from(depth_image_resource);
    light_cull_pass.reads_from(light_data_resource);
    light_cull_pass.writes_to(light_visibility_data_resource);
    light_cull_pass.set_on_record([&](const vk::CommandBuffer &cmd_buf) {
        cmd_buf.bind_pipeline(vkb::PipelineBindPoint::Compute, light_cull_pipeline);
        cmd_buf.dispatch(row_tile_count, col_tile_count, 1);
    });

    auto &deferred_pass = render_graph.add_compute_pass("Deferred pass");
    deferred_pass.reads_from(global_ubo_resource);
    deferred_pass.reads_from(albedo_image_resource);
    deferred_pass.reads_from(normal_image_resource);
    deferred_pass.reads_from(depth_image_resource);
    deferred_pass.reads_from(shadow_map_resource);
    deferred_pass.reads_from(light_data_resource);
    deferred_pass.reads_from(light_visibility_data_resource);
    deferred_pass.writes_to(swapchain_resource);
    deferred_pass.set_on_record([&](const vk::CommandBuffer &cmd_buf) {
        cmd_buf.bind_pipeline(vkb::PipelineBindPoint::Compute, deferred_pipeline);
        cmd_buf.dispatch(vull::ceil_div(window.width(), 8u), vull::ceil_div(window.height(), 8u), 1);
    });

    ui::Renderer ui(context, render_graph, swapchain, swapchain_resource, ui_vertex_shader, ui_fragment_shader);
    ui::TimeGraph cpu_time_graph(Vec2f(600.0f, 300.0f), Vec3f(0.6f, 0.7f, 0.8f));
    ui::TimeGraph gpu_time_graph(Vec2f(600.0f, 300.0f), Vec3f(0.8f, 0.0f, 0.7f));
    auto font = ui.load_font("../engine/fonts/DejaVuSansMono.ttf", 20);
    ui.set_global_scale(window.ppcm() / 37.8f * 0.55f);
    render_graph.compile(swapchain_resource);

    auto timestamp_pools = render_graph.create_timestamp_pools(context, 2);

    vkb::FenceCreateInfo fence_ci{
        .sType = vkb::StructureType::FenceCreateInfo,
        .flags = vkb::FenceCreateFlags::Signaled,
    };
    Array<vkb::Fence, 2> frame_fences;
    VULL_ENSURE(context.vkCreateFence(&fence_ci, &frame_fences[0]) == vkb::Result::Success);
    VULL_ENSURE(context.vkCreateFence(&fence_ci, &frame_fences[1]) == vkb::Result::Success);

    vkb::SemaphoreCreateInfo semaphore_ci{
        .sType = vkb::StructureType::SemaphoreCreateInfo,
    };
    Array<vkb::Semaphore, 4> frame_semaphores;
    for (auto &semaphore : frame_semaphores) {
        VULL_ENSURE(context.vkCreateSemaphore(&semaphore_ci, &semaphore) == vkb::Result::Success);
    }

    vkb::PhysicalDeviceProperties device_properties{};
    context.vkGetPhysicalDeviceProperties(&device_properties);

    auto &world = scene.world();
    world.register_component<RigidBody>();
    world.register_component<Collider>();

    for (auto [entity, mesh, transform] : world.view<Mesh, Transform>()) {
        if (strstr(mesh.vertex_data_name().data(), "Cube") == nullptr) {
            continue;
        }
        entity.add<Collider>(vull::make_unique<BoxShape>(transform.scale()));
    }

    auto player = world.create_entity();
    player.add<Transform>(~EntityId(0), Vec3f(0.0f, 10.0f, 0.0f), Quatf(), Vec3f(1.0f, 1.0f, 1.0f));
    player.add<Mesh>("/meshes/Cube.001.0/vertex", "/meshes/Cube.001.0/index");
    player.add<Material>(0u, 1u);
    player.add<RigidBody>(250.0f);
    player.add<Collider>(vull::make_unique<BoxShape>(Vec3f(1.0f, 1.0f, 1.0f)));
    player.get<RigidBody>().set_shape(player.get<Collider>().shape());

    PhysicsEngine physics_engine;
    bool free_camera_active = false;
    bool free_camera_active_key_pressed = false;
    vull::seed_rand(3);

    uint32_t frame_index = 0;
    Timer frame_timer;
    cpu_time_graph.new_bar();
    while (!window.should_close()) {
        float dt = frame_timer.elapsed();
        frame_timer.reset();

        Timer physics_timer;
        if (!window.is_key_down(Key::P)) {
            physics_engine.step(world, dt);
        }
        cpu_time_graph.push_section("Physics", physics_timer.elapsed());

        vkb::DescriptorSet frame_set = frame_sets[frame_index];
        vkb::Fence frame_fence = frame_fences[frame_index];
        vkb::Semaphore image_available_semaphore = frame_semaphores[frame_index * 2];
        vkb::Semaphore rendering_finished_semaphore = frame_semaphores[frame_index * 2 + 1];
        vk::QueryPool &timestamp_pool = timestamp_pools[frame_index];
        void *light_data = light_data_ptrs[frame_index];
        void *ubo_data = ubo_data_ptrs[frame_index];

        gpu_time_graph.new_bar();

        Timer acquire_timer;
        uint32_t image_index = swapchain.acquire_image(image_available_semaphore);
        cpu_time_graph.push_section("Acquire swapchain", acquire_timer.elapsed());

        Timer wait_fence_timer;
        context.vkWaitForFences(1, &frame_fence, true, ~0ul);
        context.vkResetFences(1, &frame_fence);
        cpu_time_graph.push_section("Wait fence", wait_fence_timer.elapsed());

        // Previous frame N's timestamp data.
        Array<uint64_t, 6> timestamp_data{};
        timestamp_pool.read_host(timestamp_data.span());
        gpu_time_graph.push_section("Geometry pass", context.timestamp_elapsed(timestamp_data[0], timestamp_data[1]));
        gpu_time_graph.push_section("Shadow pass", context.timestamp_elapsed(timestamp_data[1], timestamp_data[2]));
        gpu_time_graph.push_section("Light cull", context.timestamp_elapsed(timestamp_data[2], timestamp_data[3]));
        gpu_time_graph.push_section("Deferred pass", context.timestamp_elapsed(timestamp_data[3], timestamp_data[4]));
        gpu_time_graph.push_section("UI", context.timestamp_elapsed(timestamp_data[4], timestamp_data[5]));

        ui.draw_rect(Vec4f(0.06f, 0.06f, 0.06f, 1.0f), {100.0f, 100.0f}, {1000.0f, 25.0f});
        ui.draw_rect(Vec4f(0.06f, 0.06f, 0.06f, 0.75f), {100.0f, 125.0f}, {1000.0f, 750.0f});
        cpu_time_graph.draw(ui, {120.0f, 200.0f}, font, "CPU time");
        gpu_time_graph.draw(ui, {120.0f, 550.0f}, font, "GPU time");
        ui.draw_text(font, {0.949f, 0.96f, 0.98f}, {95.0f, 140.0f},
                     vull::format("Camera position: ({}, {}, {})", ubo.camera_position.x(), ubo.camera_position.y(),
                                  ubo.camera_position.z()));

        if (window.is_key_down(Key::F) && !free_camera_active_key_pressed) {
            free_camera_active = !free_camera_active;
            free_camera_active_key_pressed = true;
        } else if (!window.is_key_down(Key::F)) {
            free_camera_active_key_pressed = false;
        }

        if (!free_camera_active) {
            auto &player_body = player.get<RigidBody>();
            auto &player_transform = player.get<Transform>();
            auto camera_forward = vull::normalise(player_transform.position() - orbit_camera.translated());
            auto camera_right = vull::normalise(vull::cross(camera_forward, Vec3f(0.0f, 1.0f, 0.0f)));

            const float speed = window.is_key_down(Key::Shift) ? 6250.0f : 1250.0f;
            if (window.is_key_down(Key::W)) {
                player_body.apply_central_force(camera_forward * speed);
            }
            if (window.is_key_down(Key::S)) {
                player_body.apply_central_force(camera_forward * -speed);
            }
            if (window.is_key_down(Key::A)) {
                player_body.apply_central_force(camera_right * -speed);
            }
            if (window.is_key_down(Key::D)) {
                player_body.apply_central_force(camera_right * speed);
            }
            orbit_camera.set_position(player_transform.position() + Vec3f(8.0f, 3.0f, 0.0f));
            orbit_camera.set_pivot(player_transform.position());
            orbit_camera.update(window, dt);
            ubo.camera_position = orbit_camera.translated();
            ubo.view = orbit_camera.view_matrix();
        } else {
            free_camera.update(window, dt);
            ubo.camera_position = free_camera.position();
            ubo.view = free_camera.view_matrix();
        }
        update_cascades();

        if (window.is_key_down(Key::L)) {
            const auto &player_transform = player.get<Transform>();
            const auto position = player_transform.position() + player_transform.forward() * 2.0f;
            const auto force = player_transform.forward() * 2000.0f;
            auto box = world.create_entity();
            box.add<Transform>(~EntityId(0), position, Quatf(), Vec3f(0.2f));
            box.add<Mesh>("/meshes/Suzanne.0/vertex", "/meshes/Suzanne.0/index");
            box.add<Material>(0u, 1u);
            box.add<Collider>(vull::make_unique<BoxShape>(Vec3f(0.2f)));
            box.add<RigidBody>(0.2f);
            box.get<RigidBody>().set_shape(box.get<Collider>().shape());
            box.get<RigidBody>().apply_central_force(force);
            player.get<RigidBody>().apply_central_force(-force);
        }

        for (auto [entity, body, transform] : world.view<RigidBody, Transform>()) {
            if (entity == player) {
                continue;
            }
            if (vull::distance(transform.position(), player.get<Transform>().position()) >= 100.0f) {
                entity.destroy();
            }
        }

        uint32_t light_count = lights.size();
        memcpy(light_data, &light_count, sizeof(uint32_t));
        memcpy(reinterpret_cast<char *>(light_data) + 4 * sizeof(float), lights.data(), lights.size_bytes());
        memcpy(ubo_data, &ubo, sizeof(UniformBuffer));

        vkb::DescriptorImageInfo output_image_info{
            .imageView = swapchain.image_view(image_index),
            .imageLayout = vkb::ImageLayout::General,
        };
        vkb::WriteDescriptorSet output_image_write{
            .sType = vkb::StructureType::WriteDescriptorSet,
            .dstSet = frame_set,
            .dstBinding = 3,
            .descriptorCount = 1,
            .descriptorType = vkb::DescriptorType::StorageImage,
            .pImageInfo = &output_image_info,
        };
        context.vkUpdateDescriptorSets(1, &output_image_write, 0, nullptr);

        Timer record_timer;
        const auto &cmd_buf = cmd_pool.request_cmd_buf();

        Array compute_sets{frame_set, deferred_set};
        cmd_buf.bind_descriptor_sets(vkb::PipelineBindPoint::Compute, compute_pipeline_layout, compute_sets.span());

        Array graphics_sets{frame_set, geometry_set};
        cmd_buf.bind_descriptor_sets(vkb::PipelineBindPoint::Graphics, geometry_pipeline_layout, graphics_sets.span());

        vkb::Image swapchain_image = swapchain.image(image_index);
        vkb::ImageView swapchain_view = swapchain.image_view(image_index);
        global_ubo_resource.set_buffer(vull::get<0>(uniform_buffers[frame_index]));
        light_data_resource.set_buffer(vull::get<0>(light_buffers[frame_index]));
        swapchain_resource.set_image(swapchain_image, swapchain_view, swapchain_resource.full_range());

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
        render_graph.record(cmd_buf, timestamp_pool);

        vkb::ImageMemoryBarrier2 swapchain_present_barrier{
            .sType = vkb::StructureType::ImageMemoryBarrier2,
            .srcStageMask = vkb::PipelineStage2::ColorAttachmentOutput,
            .srcAccessMask = vkb::Access2::ColorAttachmentWrite,
            .oldLayout = vkb::ImageLayout::AttachmentOptimal,
            .newLayout = vkb::ImageLayout::PresentSrcKHR,
            .image = swapchain_image,
            .subresourceRange = swapchain_resource.full_range(),
        };
        cmd_buf.image_barrier(swapchain_present_barrier);

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
                .stageMask = vkb::PipelineStage2::ColorAttachmentOutput,
            },
        };
        queue.submit(cmd_buf, frame_fence, signal_semaphores.span(), wait_semaphores.span());
        cpu_time_graph.new_bar();
        cpu_time_graph.push_section("Record", record_timer.elapsed());

        Array present_wait_semaphores{rendering_finished_semaphore};
        swapchain.present(image_index, present_wait_semaphores.span());
        window.poll_events();
        frame_index = (frame_index + 1) % 2;
    }
    scheduler.stop();
    context.vkDeviceWaitIdle();
    for (auto *semaphore : frame_semaphores) {
        context.vkDestroySemaphore(semaphore);
    }
    for (auto *fence : frame_fences) {
        context.vkDestroyFence(fence);
    }
    context.vkDestroyDescriptorPool(descriptor_pool);
    context.vkFreeMemory(light_visibilities_buffer_memory);
    context.vkDestroyBuffer(light_visibilities_buffer);
    for (auto &[buffer, memory] : light_buffers) {
        context.vkDestroyBuffer(buffer);
        context.vkFreeMemory(memory);
    }
    for (auto &[buffer, memory] : uniform_buffers) {
        context.vkDestroyBuffer(buffer);
        context.vkFreeMemory(memory);
    }
    context.vkDestroySampler(normal_sampler);
    context.vkDestroySampler(albedo_sampler);
    context.vkDestroySampler(shadow_sampler);
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
    context.vkDestroyDescriptorSetLayout(frame_set_layout);
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
