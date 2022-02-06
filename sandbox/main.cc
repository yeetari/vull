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

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <vulkan/vulkan_core.h>

namespace {

VkShaderModule load_shader(const vull::Context &context, const char *path) {
    FILE *file = fopen(path, "rb");
    fseek(file, 0, SEEK_END);
    vull::LargeVector<uint32_t> binary(static_cast<size_t>(ftell(file)) / sizeof(uint32_t));
    fseek(file, 0, SEEK_SET);
    VULL_ENSURE(fread(binary.data(), sizeof(uint32_t), binary.size(), file) == binary.size());
    fclose(file);
    VkShaderModuleCreateInfo module_ci{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = binary.size_bytes(),
        .pCode = binary.data(),
    };
    VkShaderModule module;
    VULL_ENSURE(context.vkCreateShaderModule(&module_ci, &module) == VK_SUCCESS);
    return module;
}

} // namespace

int main() {
    vull::Window window(2560, 1440, true);
    vull::Context context;
    auto swapchain = window.create_swapchain(context);

    VkCommandPool command_pool = nullptr;
    VkQueue queue = nullptr;
    for (uint32_t i = 0; i < context.queue_families().size(); i++) {
        const auto &family = context.queue_families()[i];
        if ((family.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0u) {
            VkCommandPoolCreateInfo command_pool_ci{
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .queueFamilyIndex = i,
            };
            VULL_ENSURE(context.vkCreateCommandPool(&command_pool_ci, &command_pool) == VK_SUCCESS,
                        "Failed to create command pool");
            context.vkGetDeviceQueue(i, 0, &queue);
            break;
        }
    }

    VkCommandBufferAllocateInfo command_buffer_ai{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkCommandBuffer command_buffer;
    VULL_ENSURE(context.vkAllocateCommandBuffers(&command_buffer_ai, &command_buffer) == VK_SUCCESS);

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
        VkSpecializationMapEntry{
            .constantID = 0,
            .offset = offsetof(SpecialisationData, tile_size),
            .size = sizeof(SpecialisationData::tile_size),
        },
        VkSpecializationMapEntry{
            .constantID = 1,
            .offset = offsetof(SpecialisationData, tile_max_light_count),
            .size = sizeof(SpecialisationData::tile_max_light_count),
        },
        VkSpecializationMapEntry{
            .constantID = 2,
            .offset = offsetof(SpecialisationData, row_tile_count),
            .size = sizeof(SpecialisationData::row_tile_count),
        },
        VkSpecializationMapEntry{
            .constantID = 3,
            .offset = offsetof(SpecialisationData, viewport_width),
            .size = sizeof(SpecialisationData::viewport_width),
        },
        VkSpecializationMapEntry{
            .constantID = 4,
            .offset = offsetof(SpecialisationData, viewport_height),
            .size = sizeof(SpecialisationData::viewport_height),
        },
    };
    VkSpecializationInfo specialisation_info{
        .mapEntryCount = specialisation_map_entries.size(),
        .pMapEntries = specialisation_map_entries.data(),
        .dataSize = sizeof(SpecialisationData),
        .pData = &specialisation_data,
    };

    auto *light_cull_shader = load_shader(context, "engine/shaders/light_cull.comp.spv");
    auto *terrain_vertex_shader = load_shader(context, "engine/shaders/terrain.vert.spv");
    auto *terrain_fragment_shader = load_shader(context, "engine/shaders/terrain.frag.spv");
    VkPipelineShaderStageCreateInfo depth_pass_shader_stage_ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = terrain_vertex_shader,
        .pName = "main",
    };
    VkPipelineShaderStageCreateInfo light_cull_shader_stage_ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = light_cull_shader,
        .pName = "main",
        .pSpecializationInfo = &specialisation_info,
    };
    vull::Array terrain_shader_stage_cis{
        VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = terrain_vertex_shader,
            .pName = "main",
        },
        VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = terrain_fragment_shader,
            .pName = "main",
            .pSpecializationInfo = &specialisation_info,
        },
    };

    vull::Array set_bindings{
        VkDescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_ALL,
        },
        VkDescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        VkDescriptorSetLayoutBinding{
            .binding = 2,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        VkDescriptorSetLayoutBinding{
            .binding = 3,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        },
        VkDescriptorSetLayoutBinding{
            .binding = 4,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        },
    };
    VkDescriptorSetLayoutCreateInfo set_layout_ci{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = set_bindings.size(),
        .pBindings = set_bindings.data(),
    };
    VkDescriptorSetLayout set_layout;
    VULL_ENSURE(context.vkCreateDescriptorSetLayout(&set_layout_ci, &set_layout) == VK_SUCCESS);

    VkPipelineLayoutCreateInfo pipeline_layout_ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &set_layout,
    };
    VkPipelineLayout pipeline_layout;
    VULL_ENSURE(context.vkCreatePipelineLayout(&pipeline_layout_ci, &pipeline_layout) == VK_SUCCESS);

    vull::Array vertex_attribute_descriptions{
        VkVertexInputAttributeDescription{
            .location = 0,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = offsetof(vull::ChunkVertex, position),
        },
    };
    VkVertexInputBindingDescription vertex_binding_description{
        .stride = sizeof(vull::ChunkVertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
    VkPipelineVertexInputStateCreateInfo vertex_input_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &vertex_binding_description,
        .vertexAttributeDescriptionCount = vertex_attribute_descriptions.size(),
        .pVertexAttributeDescriptions = vertex_attribute_descriptions.data(),
    };
    VkPipelineInputAssemblyStateCreateInfo input_assembly_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };

    VkRect2D scissor{
        .extent = swapchain.extent_2D(),
    };
    VkViewport viewport{
        .width = static_cast<float>(window.width()),
        .height = static_cast<float>(window.height()),
        .maxDepth = 1.0f,
    };
    VkPipelineViewportStateCreateInfo viewport_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor,
    };

    VkPipelineRasterizationStateCreateInfo rasterisation_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1.0f,
    };

    VkPipelineMultisampleStateCreateInfo multisample_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .minSampleShading = 1.0f,
    };

    VkPipelineDepthStencilStateCreateInfo depth_pass_depth_stencil_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL,
    };
    VkPipelineDepthStencilStateCreateInfo terrain_pass_depth_stencil_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_EQUAL,
    };

    VkPipelineColorBlendAttachmentState terrain_pass_blend_attachment{
        // NOLINTNEXTLINE
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                          VK_COLOR_COMPONENT_A_BIT, // NOLINT
    };
    VkPipelineColorBlendStateCreateInfo terrain_pass_blend_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &terrain_pass_blend_attachment,
    };

    VkFormat depth_format = VK_FORMAT_D32_SFLOAT;
    VkPipelineRenderingCreateInfoKHR depth_pass_rendering_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
        .depthAttachmentFormat = depth_format,
        .stencilAttachmentFormat = depth_format,
    };

    VkGraphicsPipelineCreateInfo depth_pass_pipeline_ci{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
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
    VkPipeline depth_pass_pipeline;
    VULL_ENSURE(context.vkCreateGraphicsPipelines(nullptr, 1, &depth_pass_pipeline_ci, &depth_pass_pipeline) ==
                VK_SUCCESS);

    VkComputePipelineCreateInfo light_cull_pipeline_ci{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = light_cull_shader_stage_ci,
        .layout = pipeline_layout,
    };
    VkPipeline light_cull_pipeline;
    VULL_ENSURE(context.vkCreateComputePipelines(nullptr, 1, &light_cull_pipeline_ci, &light_cull_pipeline) ==
                VK_SUCCESS);

    VkFormat colour_format = VK_FORMAT_B8G8R8A8_SRGB;
    VkPipelineRenderingCreateInfoKHR terrain_pass_rendering_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &colour_format,
        .depthAttachmentFormat = depth_format,
        .stencilAttachmentFormat = depth_format,
    };

    VkGraphicsPipelineCreateInfo terrain_pass_pipeline_ci{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
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
    VkPipeline terrain_pass_pipeline;
    VULL_ENSURE(context.vkCreateGraphicsPipelines(nullptr, 1, &terrain_pass_pipeline_ci, &terrain_pass_pipeline) ==
                VK_SUCCESS);

    VkImageCreateInfo depth_image_ci{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = depth_format,
        .extent = swapchain.extent_3D(),
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VkImage depth_image;
    VULL_ENSURE(context.vkCreateImage(&depth_image_ci, &depth_image) == VK_SUCCESS);

    VkMemoryRequirements depth_image_requirements;
    context.vkGetImageMemoryRequirements(depth_image, &depth_image_requirements);
    VkDeviceMemory depth_image_memory =
        context.allocate_memory(depth_image_requirements, vull::MemoryType::DeviceLocal);
    VULL_ENSURE(context.vkBindImageMemory(depth_image, depth_image_memory, 0) == VK_SUCCESS);

    VkImageViewCreateInfo depth_image_view_ci{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = depth_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = depth_format,
        .subresourceRange{
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .levelCount = 1,
            .layerCount = 1,
        },
    };
    VkImageView depth_image_view;
    VULL_ENSURE(context.vkCreateImageView(&depth_image_view_ci, &depth_image_view) == VK_SUCCESS);

    VkSamplerCreateInfo depth_sampler_ci{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
    };
    VkSampler depth_sampler;
    VULL_ENSURE(context.vkCreateSampler(&depth_sampler_ci, &depth_sampler) == VK_SUCCESS);

    vull::Terrain terrain(2048.0f, 0);
    VkImageCreateInfo height_image_ci{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R32_SFLOAT,
        .extent = {static_cast<uint32_t>(terrain.size()), static_cast<uint32_t>(terrain.size()), 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_LINEAR,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VkImage height_image;
    VULL_ENSURE(context.vkCreateImage(&height_image_ci, &height_image) == VK_SUCCESS);

    VkMemoryRequirements height_image_requirements;
    context.vkGetImageMemoryRequirements(height_image, &height_image_requirements);
    VkDeviceMemory height_image_memory =
        context.allocate_memory(height_image_requirements, vull::MemoryType::HostVisible);
    VULL_ENSURE(context.vkBindImageMemory(height_image, height_image_memory, 0) == VK_SUCCESS);

    VkImageViewCreateInfo height_image_view_ci{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = height_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = height_image_ci.format,
        .subresourceRange{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .levelCount = 1,
            .layerCount = 1,
        },
    };
    VkImageView height_image_view;
    VULL_ENSURE(context.vkCreateImageView(&height_image_view_ci, &height_image_view) == VK_SUCCESS);

    VkSamplerCreateInfo height_sampler_ci{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
        .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
    };
    VkSampler height_sampler;
    VULL_ENSURE(context.vkCreateSampler(&height_sampler_ci, &height_sampler) == VK_SUCCESS);

    struct UniformBuffer {
        vull::Mat4f proj;
        vull::Mat4f view;
        vull::Vec3f camera_position;
        float terrain_size{0.0f};
    };
    VkBufferCreateInfo uniform_buffer_ci{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(UniformBuffer),
        .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    VkBuffer uniform_buffer;
    VULL_ENSURE(context.vkCreateBuffer(&uniform_buffer_ci, &uniform_buffer) == VK_SUCCESS);

    VkMemoryRequirements uniform_buffer_requirements;
    context.vkGetBufferMemoryRequirements(uniform_buffer, &uniform_buffer_requirements);
    VkDeviceMemory uniform_buffer_memory =
        context.allocate_memory(uniform_buffer_requirements, vull::MemoryType::HostVisible);
    VULL_ENSURE(context.vkBindBufferMemory(uniform_buffer, uniform_buffer_memory, 0) == VK_SUCCESS);

    struct PointLight {
        vull::Vec3f position;
        float radius{0.0f};
        vull::Vec3f colour;
        float padding{0.0f};
    };
    VkDeviceSize lights_buffer_size = sizeof(PointLight) * 3000 + sizeof(float) * 4;
    VkDeviceSize light_visibility_size = (specialisation_data.tile_max_light_count + 1) * sizeof(uint32_t);
    VkDeviceSize light_visibilities_buffer_size = light_visibility_size * row_tile_count * col_tile_count;

    VkBufferCreateInfo lights_buffer_ci{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = lights_buffer_size,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    VkBuffer lights_buffer;
    VULL_ENSURE(context.vkCreateBuffer(&lights_buffer_ci, &lights_buffer) == VK_SUCCESS);

    VkMemoryRequirements lights_buffer_requirements;
    context.vkGetBufferMemoryRequirements(lights_buffer, &lights_buffer_requirements);
    VkDeviceMemory lights_buffer_memory =
        context.allocate_memory(lights_buffer_requirements, vull::MemoryType::HostVisible);
    VULL_ENSURE(context.vkBindBufferMemory(lights_buffer, lights_buffer_memory, 0) == VK_SUCCESS);

    VkBufferCreateInfo light_visibilities_buffer_ci{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = light_visibilities_buffer_size,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    VkBuffer light_visibilities_buffer;
    VULL_ENSURE(context.vkCreateBuffer(&light_visibilities_buffer_ci, &light_visibilities_buffer) == VK_SUCCESS);

    VkMemoryRequirements light_visibilities_buffer_requirements;
    context.vkGetBufferMemoryRequirements(light_visibilities_buffer, &light_visibilities_buffer_requirements);
    VkDeviceMemory light_visibilities_buffer_memory =
        context.allocate_memory(light_visibilities_buffer_requirements, vull::MemoryType::DeviceLocal);
    VULL_ENSURE(context.vkBindBufferMemory(light_visibilities_buffer, light_visibilities_buffer_memory, 0) ==
                VK_SUCCESS);

    vull::Array descriptor_pool_sizes{
        VkDescriptorPoolSize{
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 4,
        },
        VkDescriptorPoolSize{
            .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 4,
        },
        VkDescriptorPoolSize{
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 2,
        },
    };
    VkDescriptorPoolCreateInfo descriptor_pool_ci{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = descriptor_pool_sizes.size(),
        .pPoolSizes = descriptor_pool_sizes.data(),
    };
    VkDescriptorPool descriptor_pool;
    VULL_ENSURE(context.vkCreateDescriptorPool(&descriptor_pool_ci, &descriptor_pool) == VK_SUCCESS);

    VkDescriptorSetAllocateInfo descriptor_set_ai{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &set_layout,
    };
    VkDescriptorSet descriptor_set;
    VULL_ENSURE(context.vkAllocateDescriptorSets(&descriptor_set_ai, &descriptor_set) == VK_SUCCESS);

    VkDescriptorBufferInfo uniform_buffer_info{
        .buffer = uniform_buffer,
        .range = VK_WHOLE_SIZE,
    };
    VkDescriptorBufferInfo lights_buffer_info{
        .buffer = lights_buffer,
        .range = VK_WHOLE_SIZE,
    };
    VkDescriptorBufferInfo light_visibilities_buffer_info{
        .buffer = light_visibilities_buffer,
        .range = VK_WHOLE_SIZE,
    };
    VkDescriptorImageInfo depth_sampler_image_info{
        .sampler = depth_sampler,
        .imageView = depth_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    VkDescriptorImageInfo height_sampler_image_info{
        .sampler = height_sampler,
        .imageView = height_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    vull::Array descriptor_writes{
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptor_set,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &uniform_buffer_info,
        },
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptor_set,
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &lights_buffer_info,
        },
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptor_set,
            .dstBinding = 2,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &light_visibilities_buffer_info,
        },
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptor_set,
            .dstBinding = 3,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &depth_sampler_image_info,
        },
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptor_set,
            .dstBinding = 4,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &height_sampler_image_info,
        },
    };
    context.vkUpdateDescriptorSets(descriptor_writes.size(), descriptor_writes.data(), 0, nullptr);

    VkFenceCreateInfo fence_ci{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    VkFence fence;
    VULL_ENSURE(context.vkCreateFence(&fence_ci, &fence) == VK_SUCCESS);

    VkSemaphoreCreateInfo semaphore_ci{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    VkSemaphore image_available_semaphore;
    VkSemaphore rendering_finished_semaphore;
    VULL_ENSURE(context.vkCreateSemaphore(&semaphore_ci, &image_available_semaphore) == VK_SUCCESS);
    VULL_ENSURE(context.vkCreateSemaphore(&semaphore_ci, &rendering_finished_semaphore) == VK_SUCCESS);

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
    context.vkMapMemory(lights_buffer_memory, 0, VK_WHOLE_SIZE, 0, &lights_data);
    context.vkMapMemory(uniform_buffer_memory, 0, VK_WHOLE_SIZE, 0, &ubo_data);

    auto get_time = [] {
        struct timespec ts {};
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return static_cast<float>(
            static_cast<double>(static_cast<uint64_t>(ts.tv_sec) * 1000000000 + static_cast<uint64_t>(ts.tv_nsec)) /
            1000000000);
    };

    VkBuffer vertex_buffer = nullptr;
    VkDeviceMemory vertex_buffer_memory = nullptr;
    VkBuffer index_buffer = nullptr;
    VkDeviceMemory index_buffer_memory = nullptr;

    float *height_data;
    context.vkMapMemory(height_image_memory, 0, VK_WHOLE_SIZE, 0, reinterpret_cast<void **>(&height_data));
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
        context.vkWaitForFences(1, &fence, VK_TRUE, ~0ul);
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

        VkBufferCreateInfo vertex_buffer_ci{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = vertices.size_bytes(),
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        VULL_ENSURE(context.vkCreateBuffer(&vertex_buffer_ci, &vertex_buffer) == VK_SUCCESS);

        VkMemoryRequirements vertex_buffer_requirements;
        context.vkGetBufferMemoryRequirements(vertex_buffer, &vertex_buffer_requirements);
        vertex_buffer_memory = context.allocate_memory(vertex_buffer_requirements, vull::MemoryType::HostVisible);
        VULL_ENSURE(context.vkBindBufferMemory(vertex_buffer, vertex_buffer_memory, 0) == VK_SUCCESS);

        void *vertex_data;
        context.vkMapMemory(vertex_buffer_memory, 0, VK_WHOLE_SIZE, 0, &vertex_data);
        memcpy(vertex_data, vertices.data(), vertices.size_bytes());
        context.vkUnmapMemory(vertex_buffer_memory);

        VkBufferCreateInfo index_buffer_ci{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = indices.size_bytes(),
            .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        VULL_ENSURE(context.vkCreateBuffer(&index_buffer_ci, &index_buffer) == VK_SUCCESS);

        VkMemoryRequirements index_buffer_requirements;
        context.vkGetBufferMemoryRequirements(index_buffer, &index_buffer_requirements);
        index_buffer_memory = context.allocate_memory(index_buffer_requirements, vull::MemoryType::HostVisible);
        VULL_ENSURE(context.vkBindBufferMemory(index_buffer, index_buffer_memory, 0) == VK_SUCCESS);

        void *index_data;
        context.vkMapMemory(index_buffer_memory, 0, VK_WHOLE_SIZE, 0, &index_data);
        memcpy(index_data, indices.data(), indices.size_bytes());
        context.vkUnmapMemory(index_buffer_memory);

        context.vkResetCommandPool(command_pool, 0);
        VkCommandBufferBeginInfo cmd_buf_bi{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        context.vkBeginCommandBuffer(command_buffer, &cmd_buf_bi);
        context.vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, 0, 1,
                                        &descriptor_set, 0, nullptr);
        context.vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1,
                                        &descriptor_set, 0, nullptr);

        vull::Array vertex_offsets{VkDeviceSize{0}};
        context.vkCmdBindVertexBuffers(command_buffer, 0, 1, &vertex_buffer, vertex_offsets.data());
        context.vkCmdBindIndexBuffer(command_buffer, index_buffer, 0, VK_INDEX_TYPE_UINT32);

        VkImageMemoryBarrier height_image_barrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_NONE_KHR,
            .dstAccessMask = VK_ACCESS_NONE_KHR,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .image = height_image,
            .subresourceRange{
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        context.vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                     VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1,
                                     &height_image_barrier);

        VkImageMemoryBarrier depth_write_barrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_NONE_KHR,
            .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            .image = depth_image,
            .subresourceRange{
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        context.vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                     VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                         VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                                     0, 0, nullptr, 0, nullptr, 1, &depth_write_barrier);

        VkRenderingAttachmentInfoKHR depth_write_attachment{
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
            .imageView = depth_image_view,
            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue{
                .depthStencil{0.0f, 0},
            },
        };
        VkRenderingInfoKHR depth_pass_rendering_info{
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
            .renderArea{
                .extent = swapchain.extent_2D(),
            },
            .layerCount = 1,
            .pDepthAttachment = &depth_write_attachment,
        };
        context.vkCmdBeginRenderingKHR(command_buffer, &depth_pass_rendering_info);
        context.vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, depth_pass_pipeline);
        context.vkCmdDrawIndexed(command_buffer, indices.size(), 1, 0, 0, 0);
        context.vkCmdEndRenderingKHR(command_buffer);

        VkImageMemoryBarrier depth_sample_barrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .image = depth_image,
            .subresourceRange{
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        context.vkCmdPipelineBarrier(
            command_buffer, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &depth_sample_barrier);
        context.vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, light_cull_pipeline);
        context.vkCmdDispatch(command_buffer, row_tile_count, col_tile_count, 1);

        vull::Array terrain_pass_buffer_barriers{
            VkBufferMemoryBarrier{
                .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                .buffer = lights_buffer,
                .size = lights_buffer_size,
            },
            VkBufferMemoryBarrier{
                .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                .buffer = light_visibilities_buffer,
                .size = light_visibilities_buffer_size,
            },
        };
        context.vkCmdPipelineBarrier(
            command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
            terrain_pass_buffer_barriers.size(), terrain_pass_buffer_barriers.data(), 0, nullptr);

        VkImageMemoryBarrier colour_write_barrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_NONE_KHR,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .image = swapchain.image(image_index),
            .subresourceRange{
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        VkImageMemoryBarrier depth_read_barrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
            .image = depth_image,
            .subresourceRange{
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        context.vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1,
                                     &colour_write_barrier);
        context.vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                     VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                         VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                                     0, 0, nullptr, 0, nullptr, 1, &depth_read_barrier);

        VkRenderingAttachmentInfoKHR colour_write_attachment{
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
            .imageView = swapchain.image_view(image_index),
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue{
                .color{{0.47f, 0.5f, 0.67f, 1.0f}},
            },
        };
        VkRenderingAttachmentInfoKHR depth_read_attachment{
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
            .imageView = depth_image_view,
            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp = VK_ATTACHMENT_STORE_OP_NONE_KHR,
        };
        VkRenderingInfoKHR terrain_pass_rendering_info{
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
            .renderArea{
                .extent = swapchain.extent_2D(),
            },
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colour_write_attachment,
            .pDepthAttachment = &depth_read_attachment,
        };
        context.vkCmdBeginRenderingKHR(command_buffer, &terrain_pass_rendering_info);
        context.vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, terrain_pass_pipeline);
        context.vkCmdDrawIndexed(command_buffer, indices.size(), 1, 0, 0, 0);
        context.vkCmdEndRenderingKHR(command_buffer);

        VkImageMemoryBarrier colour_present_barrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_NONE_KHR,
            .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .image = swapchain.image(image_index),
            .subresourceRange{
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        context.vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                     VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1,
                                     &colour_present_barrier);
        context.vkEndCommandBuffer(command_buffer);

        VkPipelineStageFlags wait_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submit_info{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
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
