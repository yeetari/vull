#include <vull/core/Window.hh>
#include <vull/maths/Common.hh>
#include <vull/maths/Mat.hh>
#include <vull/maths/Vec.hh>
#include <vull/support/Array.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Vector.hh>
#include <vull/terrain/Chunk.hh>
#include <vull/terrain/Terrain.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/Swapchain.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

namespace {

vull::vk::ShaderModule load_shader(const vull::Context &context, const char *path) {
    FILE *file = fopen(path, "rb");
    fseek(file, 0, SEEK_END);
    vull::LargeVector<uint32_t> binary(static_cast<size_t>(ftell(file)) / sizeof(uint32_t));
    fseek(file, 0, SEEK_SET);
    VULL_ENSURE(fread(binary.data(), sizeof(uint32_t), binary.size(), file) == binary.size());
    fclose(file);
    vull::vk::ShaderModuleCreateInfo module_ci{
        .sType = vull::vk::StructureType::ShaderModuleCreateInfo,
        .codeSize = binary.size_bytes(),
        .pCode = binary.data(),
    };
    vull::vk::ShaderModule module;
    VULL_ENSURE(context.vkCreateShaderModule(&module_ci, &module) == vull::vk::Result::Success);
    return module;
}

} // namespace

int main() {
    vull::Window window(2560, 1440, true);
    vull::Context context;
    auto swapchain = window.create_swapchain(context);

    vull::vk::CommandPool command_pool = nullptr;
    vull::vk::Queue queue = nullptr;
    for (uint32_t i = 0; i < context.queue_families().size(); i++) {
        const auto &family = context.queue_families()[i];
        if ((family.queueFlags & vull::vk::QueueFlags::Graphics) != vull::vk::QueueFlags::None) {
            vull::vk::CommandPoolCreateInfo command_pool_ci{
                .sType = vull::vk::StructureType::CommandPoolCreateInfo,
                .queueFamilyIndex = i,
            };
            VULL_ENSURE(context.vkCreateCommandPool(&command_pool_ci, &command_pool) == vull::vk::Result::Success,
                        "Failed to create command pool");
            context.vkGetDeviceQueue(i, 0, &queue);
            break;
        }
    }

    vull::vk::CommandBufferAllocateInfo command_buffer_ai{
        .sType = vull::vk::StructureType::CommandBufferAllocateInfo,
        .commandPool = command_pool,
        .level = vull::vk::CommandBufferLevel::Primary,
        .commandBufferCount = 1,
    };
    vull::vk::CommandBuffer command_buffer;
    VULL_ENSURE(context.vkAllocateCommandBuffers(&command_buffer_ai, &command_buffer) == vull::vk::Result::Success);

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

    vull::Array specialisation_map_entries{
        vull::vk::SpecializationMapEntry{
            .constantID = 0,
            .offset = offsetof(SpecialisationData, tile_size),
            .size = sizeof(SpecialisationData::tile_size),
        },
        vull::vk::SpecializationMapEntry{
            .constantID = 1,
            .offset = offsetof(SpecialisationData, tile_max_light_count),
            .size = sizeof(SpecialisationData::tile_max_light_count),
        },
        vull::vk::SpecializationMapEntry{
            .constantID = 2,
            .offset = offsetof(SpecialisationData, row_tile_count),
            .size = sizeof(SpecialisationData::row_tile_count),
        },
        vull::vk::SpecializationMapEntry{
            .constantID = 3,
            .offset = offsetof(SpecialisationData, viewport_width),
            .size = sizeof(SpecialisationData::viewport_width),
        },
        vull::vk::SpecializationMapEntry{
            .constantID = 4,
            .offset = offsetof(SpecialisationData, viewport_height),
            .size = sizeof(SpecialisationData::viewport_height),
        },
    };
    vull::vk::SpecializationInfo specialisation_info{
        .mapEntryCount = specialisation_map_entries.size(),
        .pMapEntries = specialisation_map_entries.data(),
        .dataSize = sizeof(SpecialisationData),
        .pData = &specialisation_data,
    };

    auto *light_cull_shader = load_shader(context, "engine/shaders/light_cull.comp.spv");
    auto *terrain_vertex_shader = load_shader(context, "engine/shaders/terrain.vert.spv");
    auto *terrain_fragment_shader = load_shader(context, "engine/shaders/terrain.frag.spv");
    vull::vk::PipelineShaderStageCreateInfo depth_pass_shader_stage_ci{
        .sType = vull::vk::StructureType::PipelineShaderStageCreateInfo,
        .stage = vull::vk::ShaderStage::Vertex,
        .module = terrain_vertex_shader,
        .pName = "main",
    };
    vull::vk::PipelineShaderStageCreateInfo light_cull_shader_stage_ci{
        .sType = vull::vk::StructureType::PipelineShaderStageCreateInfo,
        .stage = vull::vk::ShaderStage::Compute,
        .module = light_cull_shader,
        .pName = "main",
        .pSpecializationInfo = &specialisation_info,
    };
    vull::Array terrain_shader_stage_cis{
        vull::vk::PipelineShaderStageCreateInfo{
            .sType = vull::vk::StructureType::PipelineShaderStageCreateInfo,
            .stage = vull::vk::ShaderStage::Vertex,
            .module = terrain_vertex_shader,
            .pName = "main",
        },
        vull::vk::PipelineShaderStageCreateInfo{
            .sType = vull::vk::StructureType::PipelineShaderStageCreateInfo,
            .stage = vull::vk::ShaderStage::Fragment,
            .module = terrain_fragment_shader,
            .pName = "main",
            .pSpecializationInfo = &specialisation_info,
        },
    };

    vull::Array set_bindings{
        vull::vk::DescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = vull::vk::DescriptorType::UniformBuffer,
            .descriptorCount = 1,
            .stageFlags = vull::vk::ShaderStage::All,
        },
        vull::vk::DescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = vull::vk::DescriptorType::StorageBuffer,
            .descriptorCount = 1,
            .stageFlags = vull::vk::ShaderStage::Compute | vull::vk::ShaderStage::Fragment,
        },
        vull::vk::DescriptorSetLayoutBinding{
            .binding = 2,
            .descriptorType = vull::vk::DescriptorType::StorageBuffer,
            .descriptorCount = 1,
            .stageFlags = vull::vk::ShaderStage::Compute | vull::vk::ShaderStage::Fragment,
        },
        vull::vk::DescriptorSetLayoutBinding{
            .binding = 3,
            .descriptorType = vull::vk::DescriptorType::CombinedImageSampler,
            .descriptorCount = 1,
            .stageFlags = vull::vk::ShaderStage::Compute,
        },
        vull::vk::DescriptorSetLayoutBinding{
            .binding = 4,
            .descriptorType = vull::vk::DescriptorType::CombinedImageSampler,
            .descriptorCount = 1,
            .stageFlags = vull::vk::ShaderStage::Vertex | vull::vk::ShaderStage::Fragment,
        },
    };
    vull::vk::DescriptorSetLayoutCreateInfo set_layout_ci{
        .sType = vull::vk::StructureType::DescriptorSetLayoutCreateInfo,
        .bindingCount = set_bindings.size(),
        .pBindings = set_bindings.data(),
    };
    vull::vk::DescriptorSetLayout set_layout;
    VULL_ENSURE(context.vkCreateDescriptorSetLayout(&set_layout_ci, &set_layout) == vull::vk::Result::Success);

    vull::vk::PipelineLayoutCreateInfo pipeline_layout_ci{
        .sType = vull::vk::StructureType::PipelineLayoutCreateInfo,
        .setLayoutCount = 1,
        .pSetLayouts = &set_layout,
    };
    vull::vk::PipelineLayout pipeline_layout;
    VULL_ENSURE(context.vkCreatePipelineLayout(&pipeline_layout_ci, &pipeline_layout) == vull::vk::Result::Success);

    vull::Array vertex_attribute_descriptions{
        vull::vk::VertexInputAttributeDescription{
            .location = 0,
            .format = vull::vk::Format::R32G32Sfloat,
            .offset = offsetof(vull::ChunkVertex, position),
        },
    };
    vull::vk::VertexInputBindingDescription vertex_binding_description{
        .stride = sizeof(vull::ChunkVertex),
        .inputRate = vull::vk::VertexInputRate::Vertex,
    };
    vull::vk::PipelineVertexInputStateCreateInfo vertex_input_state{
        .sType = vull::vk::StructureType::PipelineVertexInputStateCreateInfo,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &vertex_binding_description,
        .vertexAttributeDescriptionCount = vertex_attribute_descriptions.size(),
        .pVertexAttributeDescriptions = vertex_attribute_descriptions.data(),
    };
    vull::vk::PipelineInputAssemblyStateCreateInfo input_assembly_state{
        .sType = vull::vk::StructureType::PipelineInputAssemblyStateCreateInfo,
        .topology = vull::vk::PrimitiveTopology::TriangleList,
    };

    vull::vk::Rect2D scissor{
        .extent = swapchain.extent_2D(),
    };
    vull::vk::Viewport viewport{
        .width = static_cast<float>(window.width()),
        .height = static_cast<float>(window.height()),
        .maxDepth = 1.0f,
    };
    vull::vk::PipelineViewportStateCreateInfo viewport_state{
        .sType = vull::vk::StructureType::PipelineViewportStateCreateInfo,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor,
    };

    vull::vk::PipelineRasterizationStateCreateInfo rasterisation_state{
        .sType = vull::vk::StructureType::PipelineRasterizationStateCreateInfo,
        .polygonMode = vull::vk::PolygonMode::Fill,
        .cullMode = vull::vk::CullMode::Back,
        .frontFace = vull::vk::FrontFace::CounterClockwise,
        .lineWidth = 1.0f,
    };

    vull::vk::PipelineMultisampleStateCreateInfo multisample_state{
        .sType = vull::vk::StructureType::PipelineMultisampleStateCreateInfo,
        .rasterizationSamples = vull::vk::SampleCount::_1,
        .minSampleShading = 1.0f,
    };

    vull::vk::PipelineDepthStencilStateCreateInfo depth_pass_depth_stencil_state{
        .sType = vull::vk::StructureType::PipelineDepthStencilStateCreateInfo,
        .depthTestEnable = vull::vk::VK_TRUE,
        .depthWriteEnable = vull::vk::VK_TRUE,
        .depthCompareOp = vull::vk::CompareOp::GreaterOrEqual,
    };
    vull::vk::PipelineDepthStencilStateCreateInfo terrain_pass_depth_stencil_state{
        .sType = vull::vk::StructureType::PipelineDepthStencilStateCreateInfo,
        .depthTestEnable = vull::vk::VK_TRUE,
        .depthCompareOp = vull::vk::CompareOp::Equal,
    };

    vull::vk::PipelineColorBlendAttachmentState terrain_pass_blend_attachment{
        .colorWriteMask = vull::vk::ColorComponent::R | vull::vk::ColorComponent::G | vull::vk::ColorComponent::B |
                          vull::vk::ColorComponent::A,
    };
    vull::vk::PipelineColorBlendStateCreateInfo terrain_pass_blend_state{
        .sType = vull::vk::StructureType::PipelineColorBlendStateCreateInfo,
        .attachmentCount = 1,
        .pAttachments = &terrain_pass_blend_attachment,
    };

    const auto depth_format = vull::vk::Format::D32Sfloat;
    vull::vk::PipelineRenderingCreateInfoKHR depth_pass_rendering_create_info{
        .sType = vull::vk::StructureType::PipelineRenderingCreateInfoKHR,
        .depthAttachmentFormat = depth_format,
        .stencilAttachmentFormat = depth_format,
    };

    vull::vk::GraphicsPipelineCreateInfo depth_pass_pipeline_ci{
        .sType = vull::vk::StructureType::GraphicsPipelineCreateInfo,
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
    vull::vk::Pipeline depth_pass_pipeline;
    VULL_ENSURE(context.vkCreateGraphicsPipelines(nullptr, 1, &depth_pass_pipeline_ci, &depth_pass_pipeline) ==
                vull::vk::Result::Success);

    vull::vk::ComputePipelineCreateInfo light_cull_pipeline_ci{
        .sType = vull::vk::StructureType::ComputePipelineCreateInfo,
        .stage = light_cull_shader_stage_ci,
        .layout = pipeline_layout,
    };
    vull::vk::Pipeline light_cull_pipeline;
    VULL_ENSURE(context.vkCreateComputePipelines(nullptr, 1, &light_cull_pipeline_ci, &light_cull_pipeline) ==
                vull::vk::Result::Success);

    const auto colour_format = vull::vk::Format::B8G8R8A8Srgb;
    vull::vk::PipelineRenderingCreateInfoKHR terrain_pass_rendering_create_info{
        .sType = vull::vk::StructureType::PipelineRenderingCreateInfoKHR,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &colour_format,
        .depthAttachmentFormat = depth_format,
        .stencilAttachmentFormat = depth_format,
    };

    vull::vk::GraphicsPipelineCreateInfo terrain_pass_pipeline_ci{
        .sType = vull::vk::StructureType::GraphicsPipelineCreateInfo,
        .pNext = &terrain_pass_rendering_create_info,
        .stageCount = terrain_shader_stage_cis.size(),
        .pStages = terrain_shader_stage_cis.data(),
        .pVertexInputState = &vertex_input_state,
        .pInputAssemblyState = &input_assembly_state,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterisation_state,
        .pMultisampleState = &multisample_state,
        .pDepthStencilState = &terrain_pass_depth_stencil_state,
        .pColorBlendState = &terrain_pass_blend_state,
        .layout = pipeline_layout,
    };
    vull::vk::Pipeline terrain_pass_pipeline;
    VULL_ENSURE(context.vkCreateGraphicsPipelines(nullptr, 1, &terrain_pass_pipeline_ci, &terrain_pass_pipeline) ==
                vull::vk::Result::Success);

    vull::vk::ImageCreateInfo depth_image_ci{
        .sType = vull::vk::StructureType::ImageCreateInfo,
        .imageType = vull::vk::ImageType::_2D,
        .format = depth_format,
        .extent = swapchain.extent_3D(),
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = vull::vk::SampleCount::_1,
        .tiling = vull::vk::ImageTiling::Optimal,
        .usage = vull::vk::ImageUsage::DepthStencilAttachment | vull::vk::ImageUsage::Sampled,
        .sharingMode = vull::vk::SharingMode::Exclusive,
        .initialLayout = vull::vk::ImageLayout::Undefined,
    };
    vull::vk::Image depth_image;
    VULL_ENSURE(context.vkCreateImage(&depth_image_ci, &depth_image) == vull::vk::Result::Success);

    vull::vk::MemoryRequirements depth_image_requirements{};
    context.vkGetImageMemoryRequirements(depth_image, &depth_image_requirements);
    vull::vk::DeviceMemory depth_image_memory =
        context.allocate_memory(depth_image_requirements, vull::MemoryType::DeviceLocal);
    VULL_ENSURE(context.vkBindImageMemory(depth_image, depth_image_memory, 0) == vull::vk::Result::Success);

    vull::vk::ImageViewCreateInfo depth_image_view_ci{
        .sType = vull::vk::StructureType::ImageViewCreateInfo,
        .image = depth_image,
        .viewType = vull::vk::ImageViewType::_2D,
        .format = depth_format,
        .subresourceRange{
            .aspectMask = vull::vk::ImageAspect::Depth,
            .levelCount = 1,
            .layerCount = 1,
        },
    };
    vull::vk::ImageView depth_image_view;
    VULL_ENSURE(context.vkCreateImageView(&depth_image_view_ci, &depth_image_view) == vull::vk::Result::Success);

    vull::vk::SamplerCreateInfo depth_sampler_ci{
        .sType = vull::vk::StructureType::SamplerCreateInfo,
        .magFilter = vull::vk::Filter::Nearest,
        .minFilter = vull::vk::Filter::Nearest,
        .mipmapMode = vull::vk::SamplerMipmapMode::Nearest,
        .addressModeU = vull::vk::SamplerAddressMode::ClampToEdge,
        .addressModeV = vull::vk::SamplerAddressMode::ClampToEdge,
        .addressModeW = vull::vk::SamplerAddressMode::ClampToEdge,
        .borderColor = vull::vk::BorderColor::FloatOpaqueWhite,
    };
    vull::vk::Sampler depth_sampler;
    VULL_ENSURE(context.vkCreateSampler(&depth_sampler_ci, &depth_sampler) == vull::vk::Result::Success);

    vull::Terrain terrain(2048.0f, 0);
    vull::vk::ImageCreateInfo height_image_ci{
        .sType = vull::vk::StructureType::ImageCreateInfo,
        .imageType = vull::vk::ImageType::_2D,
        .format = vull::vk::Format::R32Sfloat,
        .extent = {static_cast<uint32_t>(terrain.size()), static_cast<uint32_t>(terrain.size()), 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = vull::vk::SampleCount::_1,
        .tiling = vull::vk::ImageTiling::Linear,
        .usage = vull::vk::ImageUsage::Sampled,
        .sharingMode = vull::vk::SharingMode::Exclusive,
        .initialLayout = vull::vk::ImageLayout::Undefined,
    };
    vull::vk::Image height_image;
    VULL_ENSURE(context.vkCreateImage(&height_image_ci, &height_image) == vull::vk::Result::Success);

    vull::vk::MemoryRequirements height_image_requirements{};
    context.vkGetImageMemoryRequirements(height_image, &height_image_requirements);
    vull::vk::DeviceMemory height_image_memory =
        context.allocate_memory(height_image_requirements, vull::MemoryType::HostVisible);
    VULL_ENSURE(context.vkBindImageMemory(height_image, height_image_memory, 0) == vull::vk::Result::Success);

    vull::vk::ImageViewCreateInfo height_image_view_ci{
        .sType = vull::vk::StructureType::ImageViewCreateInfo,
        .image = height_image,
        .viewType = vull::vk::ImageViewType::_2D,
        .format = height_image_ci.format,
        .subresourceRange{
            .aspectMask = vull::vk::ImageAspect::Color,
            .levelCount = 1,
            .layerCount = 1,
        },
    };
    vull::vk::ImageView height_image_view;
    VULL_ENSURE(context.vkCreateImageView(&height_image_view_ci, &height_image_view) == vull::vk::Result::Success);

    vull::vk::SamplerCreateInfo height_sampler_ci{
        .sType = vull::vk::StructureType::SamplerCreateInfo,
        .magFilter = vull::vk::Filter::Linear,
        .minFilter = vull::vk::Filter::Linear,
        .mipmapMode = vull::vk::SamplerMipmapMode::Linear,
        .addressModeU = vull::vk::SamplerAddressMode::MirroredRepeat,
        .addressModeV = vull::vk::SamplerAddressMode::MirroredRepeat,
        .addressModeW = vull::vk::SamplerAddressMode::MirroredRepeat,
        .borderColor = vull::vk::BorderColor::FloatOpaqueWhite,
    };
    vull::vk::Sampler height_sampler;
    VULL_ENSURE(context.vkCreateSampler(&height_sampler_ci, &height_sampler) == vull::vk::Result::Success);

    struct UniformBuffer {
        vull::Mat4f proj;
        vull::Mat4f view;
        vull::Vec3f camera_position;
        float terrain_size{0.0f};
    };
    vull::vk::BufferCreateInfo uniform_buffer_ci{
        .sType = vull::vk::StructureType::BufferCreateInfo,
        .size = sizeof(UniformBuffer),
        .usage = vull::vk::BufferUsage::UniformBuffer,
        .sharingMode = vull::vk::SharingMode::Exclusive,
    };
    vull::vk::Buffer uniform_buffer;
    VULL_ENSURE(context.vkCreateBuffer(&uniform_buffer_ci, &uniform_buffer) == vull::vk::Result::Success);

    vull::vk::MemoryRequirements uniform_buffer_requirements{};
    context.vkGetBufferMemoryRequirements(uniform_buffer, &uniform_buffer_requirements);
    vull::vk::DeviceMemory uniform_buffer_memory =
        context.allocate_memory(uniform_buffer_requirements, vull::MemoryType::HostVisible);
    VULL_ENSURE(context.vkBindBufferMemory(uniform_buffer, uniform_buffer_memory, 0) == vull::vk::Result::Success);

    struct PointLight {
        vull::Vec3f position;
        float radius{0.0f};
        vull::Vec3f colour;
        float padding{0.0f};
    };
    vull::vk::DeviceSize lights_buffer_size = sizeof(PointLight) * 3000 + sizeof(float) * 4;
    vull::vk::DeviceSize light_visibility_size = (specialisation_data.tile_max_light_count + 1) * sizeof(uint32_t);
    vull::vk::DeviceSize light_visibilities_buffer_size = light_visibility_size * row_tile_count * col_tile_count;

    vull::vk::BufferCreateInfo lights_buffer_ci{
        .sType = vull::vk::StructureType::BufferCreateInfo,
        .size = lights_buffer_size,
        .usage = vull::vk::BufferUsage::StorageBuffer,
        .sharingMode = vull::vk::SharingMode::Exclusive,
    };
    vull::vk::Buffer lights_buffer;
    VULL_ENSURE(context.vkCreateBuffer(&lights_buffer_ci, &lights_buffer) == vull::vk::Result::Success);

    vull::vk::MemoryRequirements lights_buffer_requirements{};
    context.vkGetBufferMemoryRequirements(lights_buffer, &lights_buffer_requirements);
    vull::vk::DeviceMemory lights_buffer_memory =
        context.allocate_memory(lights_buffer_requirements, vull::MemoryType::HostVisible);
    VULL_ENSURE(context.vkBindBufferMemory(lights_buffer, lights_buffer_memory, 0) == vull::vk::Result::Success);

    vull::vk::BufferCreateInfo light_visibilities_buffer_ci{
        .sType = vull::vk::StructureType::BufferCreateInfo,
        .size = light_visibilities_buffer_size,
        .usage = vull::vk::BufferUsage::StorageBuffer,
        .sharingMode = vull::vk::SharingMode::Exclusive,
    };
    vull::vk::Buffer light_visibilities_buffer;
    VULL_ENSURE(context.vkCreateBuffer(&light_visibilities_buffer_ci, &light_visibilities_buffer) ==
                vull::vk::Result::Success);

    vull::vk::MemoryRequirements light_visibilities_buffer_requirements{};
    context.vkGetBufferMemoryRequirements(light_visibilities_buffer, &light_visibilities_buffer_requirements);
    vull::vk::DeviceMemory light_visibilities_buffer_memory =
        context.allocate_memory(light_visibilities_buffer_requirements, vull::MemoryType::DeviceLocal);
    VULL_ENSURE(context.vkBindBufferMemory(light_visibilities_buffer, light_visibilities_buffer_memory, 0) ==
                vull::vk::Result::Success);

    vull::Array descriptor_pool_sizes{
        vull::vk::DescriptorPoolSize{
            .type = vull::vk::DescriptorType::UniformBuffer,
            .descriptorCount = 1,
        },
        vull::vk::DescriptorPoolSize{
            .type = vull::vk::DescriptorType::StorageBuffer,
            .descriptorCount = 2,
        },
        vull::vk::DescriptorPoolSize{
            .type = vull::vk::DescriptorType::CombinedImageSampler,
            .descriptorCount = 2,
        },
    };
    vull::vk::DescriptorPoolCreateInfo descriptor_pool_ci{
        .sType = vull::vk::StructureType::DescriptorPoolCreateInfo,
        .maxSets = 1,
        .poolSizeCount = descriptor_pool_sizes.size(),
        .pPoolSizes = descriptor_pool_sizes.data(),
    };
    vull::vk::DescriptorPool descriptor_pool;
    VULL_ENSURE(context.vkCreateDescriptorPool(&descriptor_pool_ci, &descriptor_pool) == vull::vk::Result::Success);

    vull::vk::DescriptorSetAllocateInfo descriptor_set_ai{
        .sType = vull::vk::StructureType::DescriptorSetAllocateInfo,
        .descriptorPool = descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &set_layout,
    };
    vull::vk::DescriptorSet descriptor_set;
    VULL_ENSURE(context.vkAllocateDescriptorSets(&descriptor_set_ai, &descriptor_set) == vull::vk::Result::Success);

    vull::vk::DescriptorBufferInfo uniform_buffer_info{
        .buffer = uniform_buffer,
        .range = vull::vk::VK_WHOLE_SIZE,
    };
    vull::vk::DescriptorBufferInfo lights_buffer_info{
        .buffer = lights_buffer,
        .range = vull::vk::VK_WHOLE_SIZE,
    };
    vull::vk::DescriptorBufferInfo light_visibilities_buffer_info{
        .buffer = light_visibilities_buffer,
        .range = vull::vk::VK_WHOLE_SIZE,
    };
    vull::vk::DescriptorImageInfo depth_sampler_image_info{
        .sampler = depth_sampler,
        .imageView = depth_image_view,
        .imageLayout = vull::vk::ImageLayout::ShaderReadOnlyOptimal,
    };
    vull::vk::DescriptorImageInfo height_sampler_image_info{
        .sampler = height_sampler,
        .imageView = height_image_view,
        .imageLayout = vull::vk::ImageLayout::ShaderReadOnlyOptimal,
    };
    vull::Array descriptor_writes{
        vull::vk::WriteDescriptorSet{
            .sType = vull::vk::StructureType::WriteDescriptorSet,
            .dstSet = descriptor_set,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = vull::vk::DescriptorType::UniformBuffer,
            .pBufferInfo = &uniform_buffer_info,
        },
        vull::vk::WriteDescriptorSet{
            .sType = vull::vk::StructureType::WriteDescriptorSet,
            .dstSet = descriptor_set,
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = vull::vk::DescriptorType::StorageBuffer,
            .pBufferInfo = &lights_buffer_info,
        },
        vull::vk::WriteDescriptorSet{
            .sType = vull::vk::StructureType::WriteDescriptorSet,
            .dstSet = descriptor_set,
            .dstBinding = 2,
            .descriptorCount = 1,
            .descriptorType = vull::vk::DescriptorType::StorageBuffer,
            .pBufferInfo = &light_visibilities_buffer_info,
        },
        vull::vk::WriteDescriptorSet{
            .sType = vull::vk::StructureType::WriteDescriptorSet,
            .dstSet = descriptor_set,
            .dstBinding = 3,
            .descriptorCount = 1,
            .descriptorType = vull::vk::DescriptorType::CombinedImageSampler,
            .pImageInfo = &depth_sampler_image_info,
        },
        vull::vk::WriteDescriptorSet{
            .sType = vull::vk::StructureType::WriteDescriptorSet,
            .dstSet = descriptor_set,
            .dstBinding = 4,
            .descriptorCount = 1,
            .descriptorType = vull::vk::DescriptorType::CombinedImageSampler,
            .pImageInfo = &height_sampler_image_info,
        },
    };
    context.vkUpdateDescriptorSets(descriptor_writes.size(), descriptor_writes.data(), 0, nullptr);

    vull::vk::FenceCreateInfo fence_ci{
        .sType = vull::vk::StructureType::FenceCreateInfo,
        .flags = vull::vk::FenceCreateFlags::Signaled,
    };
    vull::vk::Fence fence;
    VULL_ENSURE(context.vkCreateFence(&fence_ci, &fence) == vull::vk::Result::Success);

    vull::vk::SemaphoreCreateInfo semaphore_ci{
        .sType = vull::vk::StructureType::SemaphoreCreateInfo,
    };
    vull::vk::Semaphore image_available_semaphore;
    vull::vk::Semaphore rendering_finished_semaphore;
    VULL_ENSURE(context.vkCreateSemaphore(&semaphore_ci, &image_available_semaphore) == vull::vk::Result::Success);
    VULL_ENSURE(context.vkCreateSemaphore(&semaphore_ci, &rendering_finished_semaphore) == vull::vk::Result::Success);

    srand(0);
    auto rand_float = [](float min, float max) {
        return min + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX) / (max - min));
    };

    vull::Vector<PointLight> lights(500);
    vull::Array<vull::Vec3f, 3000> dsts{};
    vull::Array<vull::Vec3f, 3000> srcs{};
    for (uint32_t i = 0; auto &light : lights) {
        light.colour = {1.0f};
        light.radius = rand_float(150.0f, 300.0f);
        light.position[0] = rand_float(-terrain.size(), terrain.size());
        light.position[1] = rand_float(0.0f, 200.0f);
        light.position[2] = rand_float(-terrain.size(), terrain.size());
        dsts[i] = light.position;
        auto rand = rand_float(300.0f, 500.0f);
        switch (static_cast<int>(rand_float(0, 5))) {
        case 0:
            dsts[i][0] += rand;
            break;
        case 1:
            dsts[i][1] += rand;
            break;
        case 2:
            dsts[i][2] += rand;
            break;
        case 3:
            dsts[i][0] -= rand;
            break;
        case 4:
            dsts[i][1] -= rand;
            break;
        case 5:
            dsts[i][2] -= rand;
            break;
        }
        srcs[i++] = light.position;
    }

    const float vertical_fov = 59.0f * 0.01745329251994329576923690768489f;
    UniformBuffer ubo{
        .proj = vull::projection_matrix(window.aspect_ratio(), 0.1f, vertical_fov),
        .camera_position = {3539.0f, 4286.0f, -4452.7f},
    };

    float yaw = 2.15f;
    float pitch = -0.84f;

    void *lights_data;
    void *ubo_data;
    context.vkMapMemory(lights_buffer_memory, 0, vull::vk::VK_WHOLE_SIZE, 0, &lights_data);
    context.vkMapMemory(uniform_buffer_memory, 0, vull::vk::VK_WHOLE_SIZE, 0, &ubo_data);

    auto get_time = [] {
        struct timespec ts {};
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return static_cast<float>(
            static_cast<double>(static_cast<uint64_t>(ts.tv_sec) * 1000000000 + static_cast<uint64_t>(ts.tv_nsec)) /
            1000000000);
    };

    vull::vk::Buffer vertex_buffer = nullptr;
    vull::vk::DeviceMemory vertex_buffer_memory = nullptr;
    vull::vk::Buffer index_buffer = nullptr;
    vull::vk::DeviceMemory index_buffer_memory = nullptr;

    float *height_data;
    context.vkMapMemory(height_image_memory, 0, vull::vk::VK_WHOLE_SIZE, 0, reinterpret_cast<void **>(&height_data));
    for (uint32_t z = 0; z < height_image_ci.extent.height; z++) {
        for (uint32_t x = 0; x < height_image_ci.extent.width; x++) {
            height_data[x + z * height_image_ci.extent.width] =
                terrain.height(static_cast<float>(z), static_cast<float>(x));
        }
    }
    context.vkUnmapMemory(height_image_memory);

    float previous_time = get_time();
    float fps_previous_time = get_time();
    uint32_t frame_count = 0;
    while (!window.should_close()) {
        float current_time = get_time();
        float dt = current_time - previous_time;
        previous_time = current_time;
        frame_count++;
        if (current_time - fps_previous_time >= 1.0f) {
            // NOLINTNEXTLINE
            printf("FPS: %u\n", frame_count);
            frame_count = 0;
            fps_previous_time = current_time;
        }

        uint32_t image_index = swapchain.acquire_image(image_available_semaphore);
        context.vkWaitForFences(1, &fence, vull::vk::VK_TRUE, ~0ul);
        context.vkResetFences(1, &fence);

        yaw += window.delta_x() * 0.005f;
        pitch -= window.delta_y() * 0.005f;

        constexpr vull::Vec3f up(0.0f, 1.0f, 0.0f);
        vull::Vec3f forward(vull::cos(yaw) * vull::cos(pitch), vull::sin(pitch), vull::sin(yaw) * vull::cos(pitch));
        forward = vull::normalise(forward);
        auto right = vull::normalise(vull::cross(forward, up));

        float speed = dt * 50.0f;
        if (window.is_key_down(vull::Key::Shift)) {
            speed *= 40.0f;
        }

        if (window.is_key_down(vull::Key::W)) {
            ubo.camera_position += forward * speed;
        }
        if (window.is_key_down(vull::Key::S)) {
            ubo.camera_position -= forward * speed;
        }
        if (window.is_key_down(vull::Key::A)) {
            ubo.camera_position -= right * speed;
        }
        if (window.is_key_down(vull::Key::D)) {
            ubo.camera_position += right * speed;
        }

        ubo.terrain_size = terrain.size();
        ubo.view = vull::look_at(ubo.camera_position, ubo.camera_position + forward, up);

        uint32_t light_count = lights.size();
        memcpy(lights_data, &light_count, sizeof(uint32_t));
        memcpy(reinterpret_cast<char *>(lights_data) + 4 * sizeof(float), lights.data(), lights.size_bytes());
        memcpy(ubo_data, &ubo, sizeof(UniformBuffer));

        context.vkFreeMemory(index_buffer_memory);
        context.vkDestroyBuffer(index_buffer);
        context.vkFreeMemory(vertex_buffer_memory);
        context.vkDestroyBuffer(vertex_buffer);

        vull::Vector<vull::Chunk *> chunks;
        terrain.update(ubo.camera_position, chunks);

        vull::Vector<vull::ChunkVertex> vertices;
        vull::Vector<uint32_t> indices;
        for (auto *chunk : chunks) {
            chunk->build_geometry(vertices, indices);
        }

        vull::vk::BufferCreateInfo vertex_buffer_ci{
            .sType = vull::vk::StructureType::BufferCreateInfo,
            .size = vertices.size_bytes(),
            .usage = vull::vk::BufferUsage::VertexBuffer,
            .sharingMode = vull::vk::SharingMode::Exclusive,
        };
        VULL_ENSURE(context.vkCreateBuffer(&vertex_buffer_ci, &vertex_buffer) == vull::vk::Result::Success);

        vull::vk::MemoryRequirements vertex_buffer_requirements{};
        context.vkGetBufferMemoryRequirements(vertex_buffer, &vertex_buffer_requirements);
        vertex_buffer_memory = context.allocate_memory(vertex_buffer_requirements, vull::MemoryType::HostVisible);
        VULL_ENSURE(context.vkBindBufferMemory(vertex_buffer, vertex_buffer_memory, 0) == vull::vk::Result::Success);

        void *vertex_data;
        context.vkMapMemory(vertex_buffer_memory, 0, vull::vk::VK_WHOLE_SIZE, 0, &vertex_data);
        memcpy(vertex_data, vertices.data(), vertices.size_bytes());
        context.vkUnmapMemory(vertex_buffer_memory);

        vull::vk::BufferCreateInfo index_buffer_ci{
            .sType = vull::vk::StructureType::BufferCreateInfo,
            .size = indices.size_bytes(),
            .usage = vull::vk::BufferUsage::IndexBuffer,
            .sharingMode = vull::vk::SharingMode::Exclusive,
        };
        VULL_ENSURE(context.vkCreateBuffer(&index_buffer_ci, &index_buffer) == vull::vk::Result::Success);

        vull::vk::MemoryRequirements index_buffer_requirements{};
        context.vkGetBufferMemoryRequirements(index_buffer, &index_buffer_requirements);
        index_buffer_memory = context.allocate_memory(index_buffer_requirements, vull::MemoryType::HostVisible);
        VULL_ENSURE(context.vkBindBufferMemory(index_buffer, index_buffer_memory, 0) == vull::vk::Result::Success);

        void *index_data;
        context.vkMapMemory(index_buffer_memory, 0, vull::vk::VK_WHOLE_SIZE, 0, &index_data);
        memcpy(index_data, indices.data(), indices.size_bytes());
        context.vkUnmapMemory(index_buffer_memory);

        context.vkResetCommandPool(command_pool, vull::vk::CommandPoolResetFlags::None);
        vull::vk::CommandBufferBeginInfo cmd_buf_bi{
            .sType = vull::vk::StructureType::CommandBufferBeginInfo,
            .flags = vull::vk::CommandBufferUsage::OneTimeSubmit,
        };
        context.vkBeginCommandBuffer(command_buffer, &cmd_buf_bi);
        context.vkCmdBindDescriptorSets(command_buffer, vull::vk::PipelineBindPoint::Compute, pipeline_layout, 0, 1,
                                        &descriptor_set, 0, nullptr);
        context.vkCmdBindDescriptorSets(command_buffer, vull::vk::PipelineBindPoint::Graphics, pipeline_layout, 0, 1,
                                        &descriptor_set, 0, nullptr);

        vull::Array vertex_offsets{vull::vk::DeviceSize{0}};
        context.vkCmdBindVertexBuffers(command_buffer, 0, 1, &vertex_buffer, vertex_offsets.data());
        context.vkCmdBindIndexBuffer(command_buffer, index_buffer, 0, vull::vk::IndexType::Uint32);

        vull::vk::ImageMemoryBarrier height_image_barrier{
            .sType = vull::vk::StructureType::ImageMemoryBarrier,
            .oldLayout = vull::vk::ImageLayout::Undefined,
            .newLayout = vull::vk::ImageLayout::ShaderReadOnlyOptimal,
            .image = height_image,
            .subresourceRange{
                .aspectMask = vull::vk::ImageAspect::Color,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        context.vkCmdPipelineBarrier(command_buffer, vull::vk::PipelineStage::TopOfPipe,
                                     vull::vk::PipelineStage::TopOfPipe, vull::vk::DependencyFlags::None, 0, nullptr, 0,
                                     nullptr, 1, &height_image_barrier);

        vull::vk::ImageMemoryBarrier depth_write_barrier{
            .sType = vull::vk::StructureType::ImageMemoryBarrier,
            .dstAccessMask = vull::vk::Access::DepthStencilAttachmentWrite,
            .oldLayout = vull::vk::ImageLayout::Undefined,
            .newLayout = vull::vk::ImageLayout::DepthAttachmentOptimal,
            .image = depth_image,
            .subresourceRange{
                .aspectMask = vull::vk::ImageAspect::Depth,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        context.vkCmdPipelineBarrier(command_buffer, vull::vk::PipelineStage::ComputeShader,
                                     vull::vk::PipelineStage::EarlyFragmentTests |
                                         vull::vk::PipelineStage::LateFragmentTests,
                                     vull::vk::DependencyFlags::None, 0, nullptr, 0, nullptr, 1, &depth_write_barrier);

        vull::vk::RenderingAttachmentInfoKHR depth_write_attachment{
            .sType = vull::vk::StructureType::RenderingAttachmentInfoKHR,
            .imageView = depth_image_view,
            .imageLayout = vull::vk::ImageLayout::DepthAttachmentOptimal,
            .loadOp = vull::vk::AttachmentLoadOp::Clear,
            .storeOp = vull::vk::AttachmentStoreOp::Store,
            .clearValue{
                .depthStencil{0.0f, 0},
            },
        };
        vull::vk::RenderingInfoKHR depth_pass_rendering_info{
            .sType = vull::vk::StructureType::RenderingInfoKHR,
            .renderArea{
                .extent = swapchain.extent_2D(),
            },
            .layerCount = 1,
            .pDepthAttachment = &depth_write_attachment,
        };
        context.vkCmdBeginRenderingKHR(command_buffer, &depth_pass_rendering_info);
        context.vkCmdBindPipeline(command_buffer, vull::vk::PipelineBindPoint::Graphics, depth_pass_pipeline);
        context.vkCmdDrawIndexed(command_buffer, indices.size(), 1, 0, 0, 0);
        context.vkCmdEndRenderingKHR(command_buffer);

        vull::vk::ImageMemoryBarrier depth_sample_barrier{
            .sType = vull::vk::StructureType::ImageMemoryBarrier,
            .srcAccessMask = vull::vk::Access::DepthStencilAttachmentWrite,
            .dstAccessMask = vull::vk::Access::ShaderRead,
            .oldLayout = vull::vk::ImageLayout::DepthAttachmentOptimal,
            .newLayout = vull::vk::ImageLayout::ShaderReadOnlyOptimal,
            .image = depth_image,
            .subresourceRange{
                .aspectMask = vull::vk::ImageAspect::Depth,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        context.vkCmdPipelineBarrier(
            command_buffer, vull::vk::PipelineStage::EarlyFragmentTests | vull::vk::PipelineStage::LateFragmentTests,
            vull::vk::PipelineStage::ComputeShader, vull::vk::DependencyFlags::None, 0, nullptr, 0, nullptr, 1,
            &depth_sample_barrier);
        context.vkCmdBindPipeline(command_buffer, vull::vk::PipelineBindPoint::Compute, light_cull_pipeline);
        context.vkCmdDispatch(command_buffer, row_tile_count, col_tile_count, 1);

        vull::Array terrain_pass_buffer_barriers{
            vull::vk::BufferMemoryBarrier{
                .sType = vull::vk::StructureType::BufferMemoryBarrier,
                .srcAccessMask = vull::vk::Access::ShaderWrite,
                .dstAccessMask = vull::vk::Access::ShaderRead,
                .buffer = lights_buffer,
                .size = lights_buffer_size,
            },
            vull::vk::BufferMemoryBarrier{
                .sType = vull::vk::StructureType::BufferMemoryBarrier,
                .srcAccessMask = vull::vk::Access::ShaderWrite,
                .dstAccessMask = vull::vk::Access::ShaderRead,
                .buffer = light_visibilities_buffer,
                .size = light_visibilities_buffer_size,
            },
        };
        context.vkCmdPipelineBarrier(command_buffer, vull::vk::PipelineStage::ComputeShader,
                                     vull::vk::PipelineStage::FragmentShader, vull::vk::DependencyFlags::None, 0,
                                     nullptr, terrain_pass_buffer_barriers.size(), terrain_pass_buffer_barriers.data(),
                                     0, nullptr);

        vull::vk::ImageMemoryBarrier colour_write_barrier{
            .sType = vull::vk::StructureType::ImageMemoryBarrier,
            .dstAccessMask = vull::vk::Access::ColorAttachmentWrite,
            .oldLayout = vull::vk::ImageLayout::Undefined,
            .newLayout = vull::vk::ImageLayout::ColorAttachmentOptimal,
            .image = swapchain.image(image_index),
            .subresourceRange{
                .aspectMask = vull::vk::ImageAspect::Color,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        vull::vk::ImageMemoryBarrier depth_read_barrier{
            .sType = vull::vk::StructureType::ImageMemoryBarrier,
            .srcAccessMask = vull::vk::Access::ShaderRead,
            .dstAccessMask = vull::vk::Access::DepthStencilAttachmentRead,
            .oldLayout = vull::vk::ImageLayout::ShaderReadOnlyOptimal,
            .newLayout = vull::vk::ImageLayout::DepthReadOnlyOptimal,
            .image = depth_image,
            .subresourceRange{
                .aspectMask = vull::vk::ImageAspect::Depth,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        context.vkCmdPipelineBarrier(command_buffer, vull::vk::PipelineStage::TopOfPipe,
                                     vull::vk::PipelineStage::ColorAttachmentOutput, vull::vk::DependencyFlags::None, 0,
                                     nullptr, 0, nullptr, 1, &colour_write_barrier);
        context.vkCmdPipelineBarrier(command_buffer, vull::vk::PipelineStage::ComputeShader,
                                     vull::vk::PipelineStage::EarlyFragmentTests |
                                         vull::vk::PipelineStage::LateFragmentTests,
                                     vull::vk::DependencyFlags::None, 0, nullptr, 0, nullptr, 1, &depth_read_barrier);

        vull::vk::RenderingAttachmentInfoKHR colour_write_attachment{
            .sType = vull::vk::StructureType::RenderingAttachmentInfoKHR,
            .imageView = swapchain.image_view(image_index),
            .imageLayout = vull::vk::ImageLayout::ColorAttachmentOptimal,
            .loadOp = vull::vk::AttachmentLoadOp::Clear,
            .storeOp = vull::vk::AttachmentStoreOp::Store,
            .clearValue{
                .color{{0.47f, 0.5f, 0.67f, 1.0f}},
            },
        };
        vull::vk::RenderingAttachmentInfoKHR depth_read_attachment{
            .sType = vull::vk::StructureType::RenderingAttachmentInfoKHR,
            .imageView = depth_image_view,
            .imageLayout = vull::vk::ImageLayout::DepthReadOnlyOptimal,
            .loadOp = vull::vk::AttachmentLoadOp::Load,
            .storeOp = vull::vk::AttachmentStoreOp::NoneKHR,
        };
        vull::vk::RenderingInfoKHR terrain_pass_rendering_info{
            .sType = vull::vk::StructureType::RenderingInfoKHR,
            .renderArea{
                .extent = swapchain.extent_2D(),
            },
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colour_write_attachment,
            .pDepthAttachment = &depth_read_attachment,
        };
        context.vkCmdBeginRenderingKHR(command_buffer, &terrain_pass_rendering_info);
        context.vkCmdBindPipeline(command_buffer, vull::vk::PipelineBindPoint::Graphics, terrain_pass_pipeline);
        context.vkCmdDrawIndexed(command_buffer, indices.size(), 1, 0, 0, 0);
        context.vkCmdEndRenderingKHR(command_buffer);

        vull::vk::ImageMemoryBarrier colour_present_barrier{
            .sType = vull::vk::StructureType::ImageMemoryBarrier,
            .srcAccessMask = vull::vk::Access::ColorAttachmentWrite,
            .oldLayout = vull::vk::ImageLayout::ColorAttachmentOptimal,
            .newLayout = vull::vk::ImageLayout::PresentSrcKHR,
            .image = swapchain.image(image_index),
            .subresourceRange{
                .aspectMask = vull::vk::ImageAspect::Color,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        context.vkCmdPipelineBarrier(command_buffer, vull::vk::PipelineStage::ColorAttachmentOutput,
                                     vull::vk::PipelineStage::BottomOfPipe, vull::vk::DependencyFlags::None, 0, nullptr,
                                     0, nullptr, 1, &colour_present_barrier);
        context.vkEndCommandBuffer(command_buffer);

        vull::vk::PipelineStage wait_stage_mask = vull::vk::PipelineStage::ColorAttachmentOutput;
        vull::vk::SubmitInfo submit_info{
            .sType = vull::vk::StructureType::SubmitInfo,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &image_available_semaphore,
            .pWaitDstStageMask = &wait_stage_mask,
            .commandBufferCount = 1,
            .pCommandBuffers = &command_buffer,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &rendering_finished_semaphore,
        };
        context.vkQueueSubmit(queue, 1, &submit_info, fence);
        vull::Array wait_semaphores{rendering_finished_semaphore};
        swapchain.present(image_index, wait_semaphores.span());
        window.poll_events();
    }
    context.vkDeviceWaitIdle();
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
    context.vkFreeMemory(index_buffer_memory);
    context.vkDestroyBuffer(index_buffer);
    context.vkFreeMemory(vertex_buffer_memory);
    context.vkDestroyBuffer(vertex_buffer);
    context.vkDestroySampler(height_sampler);
    context.vkDestroyImageView(height_image_view);
    context.vkFreeMemory(height_image_memory);
    context.vkDestroyImage(height_image);
    context.vkDestroySampler(depth_sampler);
    context.vkDestroyImageView(depth_image_view);
    context.vkFreeMemory(depth_image_memory);
    context.vkDestroyImage(depth_image);
    context.vkDestroyPipeline(terrain_pass_pipeline);
    context.vkDestroyPipeline(light_cull_pipeline);
    context.vkDestroyPipeline(depth_pass_pipeline);
    context.vkDestroyPipelineLayout(pipeline_layout);
    context.vkDestroyDescriptorSetLayout(set_layout);
    context.vkDestroyShaderModule(terrain_fragment_shader);
    context.vkDestroyShaderModule(terrain_vertex_shader);
    context.vkDestroyShaderModule(light_cull_shader);
    context.vkDestroyCommandPool(command_pool);
}
