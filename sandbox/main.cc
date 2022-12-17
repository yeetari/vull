#include <vull/core/Input.hh>
#include <vull/core/Material.hh>
#include <vull/core/Mesh.hh>
#include <vull/core/Scene.hh>
#include <vull/core/Transform.hh>
#include <vull/core/Window.hh>
#include <vull/ecs/Entity.hh>
#include <vull/ecs/EntityId.hh>
#include <vull/ecs/World.hh>
#include <vull/graphics/Frame.hh>
#include <vull/graphics/FramePacer.hh>
#include <vull/graphics/Vertex.hh>
#include <vull/maths/Common.hh>
#include <vull/maths/Mat.hh>
#include <vull/maths/Quat.hh>
#include <vull/maths/Random.hh>
#include <vull/maths/Vec.hh>
#include <vull/physics/Collider.hh>
#include <vull/physics/PhysicsEngine.hh>
#include <vull/physics/RigidBody.hh>
#include <vull/physics/Shape.hh>
#include <vull/platform/Timer.hh>
#include <vull/support/Array.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Format.hh>
#include <vull/support/HashMap.hh>
#include <vull/support/HashSet.hh>
#include <vull/support/Result.hh>
#include <vull/support/Span.hh>
#include <vull/support/String.hh>
#include <vull/support/StringView.hh>
#include <vull/support/Tuple.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>
#include <vull/tasklet/Scheduler.hh>
#include <vull/tasklet/Tasklet.hh> // IWYU pragma: keep
#include <vull/ui/Renderer.hh>
#include <vull/ui/TimeGraph.hh>
#include <vull/vulkan/Buffer.hh>
#include <vull/vulkan/CommandBuffer.hh>
#include <vull/vulkan/CommandPool.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/Fence.hh>
#include <vull/vulkan/Image.hh>
#include <vull/vulkan/ImageView.hh>
#include <vull/vulkan/MemoryUsage.hh>
#include <vull/vulkan/Queue.hh>
#include <vull/vulkan/RenderGraph.hh>
#include <vull/vulkan/Semaphore.hh>
#include <vull/vulkan/Shader.hh>
#include <vull/vulkan/Swapchain.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

Vector<uint8_t> load(const char *path) {
    FILE *file = fopen(path, "rb");
    fseek(file, 0, SEEK_END);
    Vector<uint8_t> binary(static_cast<uint32_t>(ftell(file)));
    fseek(file, 0, SEEK_SET);
    VULL_ENSURE(fread(binary.data(), 1, binary.size(), file) == binary.size());
    fclose(file);
    return binary;
}

void main_task(Scheduler &scheduler, StringView scene_name) {
    Window window(2560, 1440, true);
    vk::Context context;
    auto swapchain = window.create_swapchain(context, vk::SwapchainMode::LowPower);

    const auto graphics_family_index = find_graphics_family(context);
    vk::CommandPool cmd_pool(context, graphics_family_index);
    vk::Queue queue(context, graphics_family_index);

    Scene scene(context);
    scene.load(cmd_pool, queue, "scene.vpak", scene_name);

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

    auto default_vs = VULL_EXPECT(vk::Shader::parse(context, load("engine/shaders/default.vert.spv").span()));
    auto default_fs = VULL_EXPECT(vk::Shader::parse(context, load("engine/shaders/default.frag.spv").span()));
    auto deferred_shader = VULL_EXPECT(vk::Shader::parse(context, load("engine/shaders/deferred.comp.spv").span()));
    auto light_cull_shader = VULL_EXPECT(vk::Shader::parse(context, load("engine/shaders/light_cull.comp.spv").span()));
    auto shadow_shader = VULL_EXPECT(vk::Shader::parse(context, load("engine/shaders/shadow.vert.spv").span()));
    auto ui_vs = VULL_EXPECT(vk::Shader::parse(context, load("engine/shaders/ui.vert.spv").span()));
    auto ui_fs = VULL_EXPECT(vk::Shader::parse(context, load("engine/shaders/ui.frag.spv").span()));

    Array geometry_pass_shader_stage_cis{
        default_vs.create_info(specialisation_info),
        default_fs.create_info(specialisation_info),
    };
    auto shadow_shader_stage_ci = shadow_shader.create_info(specialisation_info);

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
    vkb::DescriptorSetLayout static_set_layout;
    VULL_ENSURE(context.vkCreateDescriptorSetLayout(&static_set_layout_ci, &static_set_layout) == vkb::Result::Success);

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
    };
    vkb::DescriptorSetLayoutCreateInfo dynamic_set_layout_ci{
        .sType = vkb::StructureType::DescriptorSetLayoutCreateInfo,
        .flags = vkb::DescriptorSetLayoutCreateFlags::DescriptorBufferEXT,
        .bindingCount = dynamic_set_bindings.size(),
        .pBindings = dynamic_set_bindings.data(),
    };
    vkb::DescriptorSetLayout dynamic_set_layout;
    VULL_ENSURE(context.vkCreateDescriptorSetLayout(&dynamic_set_layout_ci, &dynamic_set_layout) ==
                vkb::Result::Success);

    vkb::DescriptorSetLayoutBinding texture_set_binding{
        .binding = 0,
        .descriptorType = vkb::DescriptorType::CombinedImageSampler,
        .descriptorCount = scene.texture_count(),
        .stageFlags = vkb::ShaderStage::Fragment,
    };
    vkb::DescriptorSetLayoutCreateInfo texture_set_layout_ci{
        .sType = vkb::StructureType::DescriptorSetLayoutCreateInfo,
        .flags = vkb::DescriptorSetLayoutCreateFlags::DescriptorBufferEXT,
        .bindingCount = 1,
        .pBindings = &texture_set_binding,
    };
    vkb::DescriptorSetLayout texture_set_layout;
    VULL_ENSURE(context.vkCreateDescriptorSetLayout(&texture_set_layout_ci, &texture_set_layout) ==
                vkb::Result::Success);

    vkb::PushConstantRange push_constant_range{
        .stageFlags = vkb::ShaderStage::AllGraphics,
        .size = sizeof(PushConstantBlock),
    };
    Array geometry_set_layouts{
        dynamic_set_layout,
        texture_set_layout,
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
        dynamic_set_layout,
        static_set_layout,
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
        .flags = vkb::PipelineCreateFlags::DescriptorBufferEXT,
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
        .flags = vkb::PipelineCreateFlags::DescriptorBufferEXT,
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
        .flags = vkb::PipelineCreateFlags::DescriptorBufferEXT,
        .stage = light_cull_shader.create_info(specialisation_info),
        .layout = compute_pipeline_layout,
    };
    vkb::Pipeline light_cull_pipeline;
    VULL_ENSURE(context.vkCreateComputePipelines(nullptr, 1, &light_cull_pipeline_ci, &light_cull_pipeline) ==
                vkb::Result::Success);

    vkb::ComputePipelineCreateInfo deferred_pipeline_ci{
        .sType = vkb::StructureType::ComputePipelineCreateInfo,
        .flags = vkb::PipelineCreateFlags::DescriptorBufferEXT,
        .stage = deferred_shader.create_info(specialisation_info),
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
    auto depth_image = context.create_image(depth_image_ci, vk::MemoryUsage::DeviceOnly);

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
    auto albedo_image = context.create_image(albedo_image_ci, vk::MemoryUsage::DeviceOnly);

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
    auto normal_image = context.create_image(normal_image_ci, vk::MemoryUsage::DeviceOnly);

    constexpr uint32_t shadow_cascade_count = 4;
    vkb::ImageCreateInfo shadow_map_image_ci{
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
    auto shadow_map_image = context.create_image(shadow_map_image_ci, vk::MemoryUsage::DeviceOnly);

    Vector<vk::ImageView> shadow_cascade_views;
    for (uint32_t i = 0; i < shadow_cascade_count; i++) {
        shadow_cascade_views.push(shadow_map_image.create_layer_view(i, vkb::ImageUsage::Sampled));
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
    struct PointLight {
        Vec3f position;
        float radius{0.0f};
        Vec3f colour;
        float padding{0.0f};
    };
    vkb::DeviceSize light_buffer_size = sizeof(PointLight) * 3000 + sizeof(float) * 4;
    vkb::DeviceSize light_visibility_size = (specialisation_data.tile_max_light_count + 1) * sizeof(uint32_t);
    vkb::DeviceSize light_visibility_buffer_size = light_visibility_size * row_tile_count * col_tile_count;

    Array uniform_buffers{
        context.create_buffer(sizeof(UniformBuffer),
                              vkb::BufferUsage::UniformBuffer | vkb::BufferUsage::ShaderDeviceAddress,
                              vk::MemoryUsage::HostToDevice),
        context.create_buffer(sizeof(UniformBuffer),
                              vkb::BufferUsage::UniformBuffer | vkb::BufferUsage::ShaderDeviceAddress,
                              vk::MemoryUsage::HostToDevice),
    };
    Array light_buffers{
        context.create_buffer(light_buffer_size,
                              vkb::BufferUsage::StorageBuffer | vkb::BufferUsage::ShaderDeviceAddress,
                              vk::MemoryUsage::HostToDevice),
        context.create_buffer(light_buffer_size,
                              vkb::BufferUsage::StorageBuffer | vkb::BufferUsage::ShaderDeviceAddress,
                              vk::MemoryUsage::HostToDevice),
    };
    auto light_visibility_buffer = context.create_buffer(
        light_visibility_buffer_size, vkb::BufferUsage::StorageBuffer | vkb::BufferUsage::ShaderDeviceAddress,
        vk::MemoryUsage::DeviceOnly);

    Vector<PointLight> lights(50);
    for (auto &light : lights) {
        light.colour = vull::linear_rand(Vec3f(0.1f), Vec3f(1.0f));
        light.radius = vull::linear_rand(2.5f, 15.0f);
        light.position = vull::linear_rand(Vec3f(-50.0f, 2.0f, -70.0f), Vec3f(100.0f, 30.0f, 50.0f));
    }

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

    vkb::DeviceSize static_set_layout_size;
    vkb::DeviceSize dynamic_set_layout_size;
    vkb::DeviceSize texture_set_layout_size;
    context.vkGetDescriptorSetLayoutSizeEXT(static_set_layout, &static_set_layout_size);
    context.vkGetDescriptorSetLayoutSizeEXT(dynamic_set_layout, &dynamic_set_layout_size);
    context.vkGetDescriptorSetLayoutSizeEXT(texture_set_layout, &texture_set_layout_size);

    auto static_descriptor_buffer = context.create_buffer(
        static_set_layout_size + texture_set_layout_size,
        vkb::BufferUsage::SamplerDescriptorBufferEXT | vkb::BufferUsage::ResourceDescriptorBufferEXT |
            vkb::BufferUsage::ShaderDeviceAddress | vkb::BufferUsage::TransferDst,
        vk::MemoryUsage::DeviceOnly);
    auto descriptor_staging_buffer = context.create_buffer(static_set_layout_size + texture_set_layout_size,
                                                           vkb::BufferUsage::TransferSrc, vk::MemoryUsage::HostOnly);
    auto *desc_ptr = descriptor_staging_buffer.mapped<uint8_t>();

    auto put_desc = [&context](uint8_t *&desc_ptr, vkb::DescriptorType type, void *info) {
        const auto size = context.descriptor_size(type);
        vkb::DescriptorGetInfoEXT get_info{
            .sType = vkb::StructureType::DescriptorGetInfoEXT,
            .type = type,
            .data{
                .pSampler = static_cast<vkb::Sampler *>(info),
            },
        };
        context.vkGetDescriptorEXT(&get_info, size, desc_ptr);
        desc_ptr += size;
    };

    vkb::DescriptorImageInfo depth_image_info{
        .imageView = *depth_image.full_view(),
        .imageLayout = vkb::ImageLayout::ReadOnlyOptimal,
    };
    vkb::DescriptorImageInfo albedo_image_info{
        .imageView = *albedo_image.full_view(),
        .imageLayout = vkb::ImageLayout::ReadOnlyOptimal,
    };
    vkb::DescriptorImageInfo normal_image_info{
        .imageView = *normal_image.full_view(),
        .imageLayout = vkb::ImageLayout::ReadOnlyOptimal,
    };
    vkb::DescriptorImageInfo shadow_map_image_info{
        .sampler = shadow_sampler,
        .imageView = *shadow_map_image.full_view(),
        .imageLayout = vkb::ImageLayout::ReadOnlyOptimal,
    };
    vkb::DescriptorAddressInfoEXT light_visibility_buffer_info{
        .sType = vkb::StructureType::DescriptorAddressInfoEXT,
        .address = light_visibility_buffer.device_address(),
        .range = light_visibility_buffer_size,
    };
    put_desc(desc_ptr, vkb::DescriptorType::SampledImage, &depth_image_info);
    put_desc(desc_ptr, vkb::DescriptorType::SampledImage, &albedo_image_info);
    put_desc(desc_ptr, vkb::DescriptorType::SampledImage, &normal_image_info);
    put_desc(desc_ptr, vkb::DescriptorType::CombinedImageSampler, &shadow_map_image_info);
    put_desc(desc_ptr, vkb::DescriptorType::StorageBuffer, &light_visibility_buffer_info);

    for (uint32_t i = 0; i < scene.texture_count(); i++) {
        vkb::DescriptorImageInfo image_info{
            .sampler = scene.texture_samplers()[i],
            .imageView = *scene.texture_images()[i].full_view(),
            .imageLayout = vkb::ImageLayout::ReadOnlyOptimal,
        };
        put_desc(desc_ptr, vkb::DescriptorType::CombinedImageSampler, &image_info);
    }

    queue.immediate_submit(cmd_pool, [&](const vk::CommandBuffer &cmd_buf) {
        vkb::BufferCopy copy{
            .size = static_set_layout_size + texture_set_layout_size,
        };
        cmd_buf.copy_buffer(*descriptor_staging_buffer, *static_descriptor_buffer, copy);
    });

    Array dynamic_descriptor_buffers{
        context.create_buffer(dynamic_set_layout_size,
                              vkb::BufferUsage::SamplerDescriptorBufferEXT |
                                  vkb::BufferUsage::ResourceDescriptorBufferEXT | vkb::BufferUsage::ShaderDeviceAddress,
                              vk::MemoryUsage::HostToDevice),
        context.create_buffer(dynamic_set_layout_size,
                              vkb::BufferUsage::SamplerDescriptorBufferEXT |
                                  vkb::BufferUsage::ResourceDescriptorBufferEXT | vkb::BufferUsage::ShaderDeviceAddress,
                              vk::MemoryUsage::HostToDevice),
    };

    vk::RenderGraph render_graph;

    // GBuffer resources.
    auto &albedo_image_resource = render_graph.add_image("GBuffer albedo");
    auto &normal_image_resource = render_graph.add_image("GBuffer normal");
    auto &depth_image_resource = render_graph.add_image("GBuffer depth");
    albedo_image_resource.set_image(*albedo_image, *albedo_image.full_view(), albedo_image.full_view().range());
    normal_image_resource.set_image(*normal_image, *normal_image.full_view(), normal_image.full_view().range());
    depth_image_resource.set_image(*depth_image, *depth_image.full_view(), depth_image.full_view().range());

    auto &shadow_map_resource = render_graph.add_image("Shadow map");
    shadow_map_resource.set_image(*shadow_map_image, *shadow_map_image.full_view(),
                                  shadow_map_image.full_view().range());

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
    light_visibility_data_resource.set_buffer(*light_visibility_buffer);

    auto &geometry_pass = render_graph.add_graphics_pass("Geometry pass");
    geometry_pass.reads_from(global_ubo_resource);
    geometry_pass.writes_to(albedo_image_resource);
    geometry_pass.writes_to(normal_image_resource);
    geometry_pass.writes_to(depth_image_resource);
    geometry_pass.set_on_record([&](vk::CommandBuffer &cmd_buf) {
        Array colour_write_attachments{
            vkb::RenderingAttachmentInfo{
                .sType = vkb::StructureType::RenderingAttachmentInfo,
                .imageView = *albedo_image.full_view(),
                .imageLayout = vkb::ImageLayout::ColorAttachmentOptimal,
                .loadOp = vkb::AttachmentLoadOp::Clear,
                .storeOp = vkb::AttachmentStoreOp::Store,
                .clearValue{
                    .color{{0.0f, 0.0f, 0.0f, 0.0f}},
                },
            },
            vkb::RenderingAttachmentInfo{
                .sType = vkb::StructureType::RenderingAttachmentInfo,
                .imageView = *normal_image.full_view(),
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
            .imageView = *depth_image.full_view(),
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
        scene.render(cmd_buf, 0);
        cmd_buf.end_rendering();
    });

    auto &shadow_pass = render_graph.add_graphics_pass("Shadow pass");
    shadow_pass.reads_from(global_ubo_resource);
    shadow_pass.writes_to(shadow_map_resource);
    shadow_pass.set_on_record([&](vk::CommandBuffer &cmd_buf) {
        cmd_buf.bind_pipeline(vkb::PipelineBindPoint::Graphics, shadow_pass_pipeline);
        for (uint32_t i = 0; i < shadow_cascade_count; i++) {
            vkb::RenderingAttachmentInfo shadow_map_write_attachment{
                .sType = vkb::StructureType::RenderingAttachmentInfo,
                .imageView = *shadow_cascade_views[i],
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
            scene.render(cmd_buf, i);
            cmd_buf.end_rendering();
        }
    });

    auto &light_cull_pass = render_graph.add_compute_pass("Light cull");
    light_cull_pass.reads_from(global_ubo_resource);
    light_cull_pass.reads_from(depth_image_resource);
    light_cull_pass.reads_from(light_data_resource);
    light_cull_pass.writes_to(light_visibility_data_resource);
    light_cull_pass.set_on_record([&](vk::CommandBuffer &cmd_buf) {
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
    deferred_pass.set_on_record([&](vk::CommandBuffer &cmd_buf) {
        cmd_buf.bind_pipeline(vkb::PipelineBindPoint::Compute, deferred_pipeline);
        cmd_buf.dispatch(vull::ceil_div(window.width(), 8u), vull::ceil_div(window.height(), 8u), 1);
    });

    ui::Renderer ui(context, render_graph, swapchain, swapchain_resource, ui_vs, ui_fs);
    ui::TimeGraph cpu_time_graph(Vec2f(600.0f, 300.0f), Vec3f(0.7f, 0.2f, 0.3f));
    ui::TimeGraph gpu_time_graph(Vec2f(600.0f, 300.0f), Vec3f(0.8f, 0.0f, 0.7f));
    auto font = ui.load_font("../engine/fonts/DejaVuSansMono.ttf", 20);
    ui.set_global_scale(window.ppcm() / 37.8f * 0.55f);
    render_graph.compile(swapchain_resource);

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
    player.add<Transform>(~EntityId(0), Vec3f(0.0f, 10.0f, 0.0f), Quatf(), Vec3f(0.5f, 1.5f, 0.5f));
    player.add<Mesh>("/meshes/Cube.001.0/vertex", "/meshes/Cube.001.0/index");
    player.add<Material>("/default_albedo", "/default_normal");
    player.add<RigidBody>(250.0f);
    player.add<Collider>(vull::make_unique<BoxShape>(player.get<Transform>().scale()));
    player.get<RigidBody>().set_ignore_rotation(true);
    player.get<RigidBody>().set_shape(player.get<Collider>().shape());

    bool free_camera_active = false;
    window.on_key_release(Key::F, [&](ModifierMask) {
        free_camera_active = !free_camera_active;
    });

    window.on_key_press(Key::Space, [&](ModifierMask) {
        float impulse = vull::sqrt(-2.0f * 6.0f * 250.0f * -9.81f * 100.0f);
        player.get<RigidBody>().apply_impulse({0.0f, impulse, 0.0f}, {});
    });

    bool mouse_visible = false;
    window.on_mouse_release(Button::Middle, [&](Vec2f) {
        mouse_visible = !mouse_visible;
        mouse_visible ? window.show_cursor() : window.hide_cursor();
    });

    float camera_pitch = 0.0f;
    float camera_yaw = 0.0f;
    window.on_mouse_move([&](Vec2f delta, Vec2f, ButtonMask) {
        camera_yaw -= delta.x() * (2.0f / static_cast<float>(window.width()));
        camera_pitch += delta.y() * (1.0f / static_cast<float>(window.height()));
        camera_pitch = vull::clamp(camera_pitch, -vull::half_pi<float> + 0.001f, vull::half_pi<float> - 0.001f);
        camera_yaw = vull::fmod(camera_yaw, vull::pi<float> * 2.0f);
    });

    FramePacer frame_pacer(swapchain, 2);
    PhysicsEngine physics_engine;
    vull::seed_rand(5);

    float fire_time = 0.0f;

    Timer frame_timer;
    cpu_time_graph.new_bar();
    while (!window.should_close()) {
        Timer acquire_frame_timer;
        auto &frame = frame_pacer.next_frame();
        cpu_time_graph.push_section("Acquire frame", acquire_frame_timer.elapsed());

        float dt = frame_timer.elapsed();
        frame_timer.reset();

        if (window.is_button_pressed(Button::Right)) {
            dt /= 5.0f;
        }

        // Poll input.
        window.poll_events();

        // Collect previous frame N's timestamp data.
        const auto pass_times = frame.pass_times(render_graph);
        gpu_time_graph.new_bar();
        for (const auto &[name, time] : pass_times) {
            gpu_time_graph.push_section(name, time);
        }

        Timer physics_timer;
        physics_engine.step(world, dt);
        cpu_time_graph.push_section("Physics", physics_timer.elapsed());

        ui.draw_rect(Vec4f(0.06f, 0.06f, 0.06f, 1.0f), {100.0f, 100.0f}, {1000.0f, 25.0f});
        ui.draw_rect(Vec4f(0.06f, 0.06f, 0.06f, 0.75f), {100.0f, 125.0f}, {1000.0f, 750.0f});
        cpu_time_graph.draw(ui, {120.0f, 200.0f}, font, "CPU time");
        gpu_time_graph.draw(ui, {120.0f, 550.0f}, font, "GPU time");
        ui.draw_text(font, {0.949f, 0.96f, 0.98f}, {95.0f, 140.0f},
                     vull::format("Camera position: ({}, {}, {}) {} {}", ubo.camera_position.x(),
                                  ubo.camera_position.y(), ubo.camera_position.z(), camera_pitch, camera_yaw));

        auto &player_body = player.get<RigidBody>();
        auto &player_transform = player.get<Transform>();

        player_transform.set_rotation(vull::angle_axis(camera_yaw, Vec3f(0.0f, 1.0f, 0.0f)));

        Vec3f camera_forward =
            vull::rotate(player_transform.rotation() * vull::angle_axis(camera_pitch, Vec3f(1.0f, 0.0f, 0.0f)),
                         Vec3f(0.0f, 0.0f, 1.0f));
        ubo.camera_position = player_transform.position() + Vec3f(0.0f, 1.5f, 0.0f);
        ubo.view = vull::look_at(ubo.camera_position, ubo.camera_position + camera_forward, Vec3f(0.0f, 1.0f, 0.0f));

        player_body.apply_central_force(player_body.linear_velocity() * Vec3f(-1000.0f, 0.0f, -1000.0f));

        const float speed = window.is_key_pressed(Key::Shift) ? 6250.0f : 1250.0f;
        if (window.is_key_pressed(Key::W)) {
            player_body.apply_central_force(player_transform.forward() * speed);
        }
        if (window.is_key_pressed(Key::S)) {
            player_body.apply_central_force(player_transform.forward() * -speed);
        }
        if (window.is_key_pressed(Key::A)) {
            player_body.apply_central_force(player_transform.right() * speed);
        }
        if (window.is_key_pressed(Key::D)) {
            player_body.apply_central_force(player_transform.right() * -speed);
        }

        update_cascades();
        if (window.is_button_pressed(Button::Left) && fire_time >= 0.1f) {
            constexpr float bullet_mass = 0.2f;
            const auto spawn_point = Vec3f(0.0f, 1.0f, 0.0f) + camera_forward * 2.0f;
            auto box = world.create_entity();
            box.add<Transform>(~EntityId(0), player_transform.position() + spawn_point, Quatf(), Vec3f(0.2f));
            box.add<Mesh>("/meshes/Suzanne.0/vertex", "/meshes/Suzanne.0/index");
            box.add<Material>("/default_albedo", "/default_normal");
            box.add<Collider>(vull::make_unique<BoxShape>(Vec3f(0.2f)));
            box.add<RigidBody>(bullet_mass);
            box.get<RigidBody>().set_shape(box.get<Collider>().shape());
            box.get<RigidBody>().apply_impulse(camera_forward * 5.0f, Vec3f(0.0f));
            box.get<RigidBody>().apply_impulse(player.get<RigidBody>().velocity_at_point(spawn_point) * bullet_mass,
                                               Vec3f(0.0f));
            fire_time = 0.0f;
        }
        fire_time += dt;

        for (auto [entity, body, transform] : world.view<RigidBody, Transform>()) {
            if (entity == player) {
                continue;
            }
            if (vull::distance(transform.position(), player.get<Transform>().position()) >= 100.0f) {
                entity.destroy();
            }
        }

        const auto frame_index = frame_pacer.frame_index();
        void *light_data = light_buffers[frame_index].mapped_raw();
        void *ubo_data = uniform_buffers[frame_index].mapped_raw();

        uint32_t light_count = lights.size();
        memcpy(light_data, &light_count, sizeof(uint32_t));
        memcpy(reinterpret_cast<char *>(light_data) + 4 * sizeof(float), lights.data(), lights.size_bytes());
        memcpy(ubo_data, &ubo, sizeof(UniformBuffer));

        auto &dynamic_descriptor_buffer = dynamic_descriptor_buffers[frame_index];
        auto *desc_data = dynamic_descriptor_buffer.mapped<uint8_t>();
        vkb::DescriptorAddressInfoEXT ubo_address_info{
            .sType = vkb::StructureType::DescriptorAddressInfoEXT,
            .address = uniform_buffers[frame_index].device_address(),
            .range = sizeof(UniformBuffer),
        };
        vkb::DescriptorAddressInfoEXT light_buffer_address_info{
            .sType = vkb::StructureType::DescriptorAddressInfoEXT,
            .address = light_buffers[frame_index].device_address(),
            .range = light_buffer_size,
        };
        const auto image_index = frame_pacer.image_index();
        vkb::DescriptorImageInfo output_image_info{
            .imageView = swapchain.image_view(image_index),
            .imageLayout = vkb::ImageLayout::General,
        };
        put_desc(desc_data, vkb::DescriptorType::UniformBuffer, &ubo_address_info);
        put_desc(desc_data, vkb::DescriptorType::StorageBuffer, &light_buffer_address_info);
        put_desc(desc_data, vkb::DescriptorType::StorageImage, &output_image_info);

        Timer record_timer;
        auto &cmd_buf = cmd_pool.request_cmd_buf();

        vkb::Image swapchain_image = swapchain.image(image_index);
        vkb::ImageView swapchain_view = swapchain.image_view(image_index);
        global_ubo_resource.set_buffer(*uniform_buffers[frame_index]);
        light_data_resource.set_buffer(*light_buffers[frame_index]);
        swapchain_resource.set_image(swapchain_image, swapchain_view, swapchain_resource.full_range());

        cmd_buf.bind_layout(vkb::PipelineBindPoint::Compute, compute_pipeline_layout);
        cmd_buf.bind_layout(vkb::PipelineBindPoint::Graphics, geometry_pipeline_layout);
        cmd_buf.bind_descriptor_buffer(vkb::PipelineBindPoint::Compute, dynamic_descriptor_buffer, 0, 0);
        cmd_buf.bind_descriptor_buffer(vkb::PipelineBindPoint::Compute, static_descriptor_buffer, 1, 0);
        cmd_buf.bind_descriptor_buffer(vkb::PipelineBindPoint::Graphics, dynamic_descriptor_buffer, 0, 0);
        cmd_buf.bind_descriptor_buffer(vkb::PipelineBindPoint::Graphics, static_descriptor_buffer, 1,
                                       static_set_layout_size);

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
        render_graph.record(cmd_buf, frame.timestamp_pool());

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
                .semaphore = *frame.present_semaphore(),
            },
        };
        Array wait_semaphores{
            vkb::SemaphoreSubmitInfo{
                .sType = vkb::StructureType::SemaphoreSubmitInfo,
                .semaphore = *frame.acquire_semaphore(),
                .stageMask = vkb::PipelineStage2::ColorAttachmentOutput,
            },
        };
        queue.submit(cmd_buf, *frame.fence(), signal_semaphores.span(), wait_semaphores.span());
        cpu_time_graph.new_bar();
        cpu_time_graph.push_section("Record", record_timer.elapsed());
    }
    scheduler.stop();
    context.vkDeviceWaitIdle();
    context.vkDestroySampler(shadow_sampler);
    context.vkDestroyPipeline(deferred_pipeline);
    context.vkDestroyPipeline(light_cull_pipeline);
    context.vkDestroyPipeline(shadow_pass_pipeline);
    context.vkDestroyPipeline(geometry_pass_pipeline);
    context.vkDestroyPipelineLayout(compute_pipeline_layout);
    context.vkDestroyPipelineLayout(geometry_pipeline_layout);
    context.vkDestroyDescriptorSetLayout(texture_set_layout);
    context.vkDestroyDescriptorSetLayout(dynamic_set_layout);
    context.vkDestroyDescriptorSetLayout(static_set_layout);
}

} // namespace

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <scene-name>\n", argv[0]);
        return EXIT_FAILURE;
    }

    Scheduler scheduler;
    scheduler.start([&] {
        main_task(scheduler, argv[1]);
    });
}
