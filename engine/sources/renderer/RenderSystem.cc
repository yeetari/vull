#include <vull/renderer/RenderSystem.hh>

#include <vull/Config.hh>
#include <vull/core/Entity.hh>
#include <vull/core/Transform.hh>
#include <vull/core/World.hh>
#include <vull/io/Window.hh>
#include <vull/renderer/Buffer.hh>
#include <vull/renderer/Device.hh>
#include <vull/renderer/Image.hh>
#include <vull/renderer/Mesh.hh>
#include <vull/renderer/PointLight.hh>
#include <vull/renderer/Swapchain.hh>
#include <vull/renderer/UniformBuffer.hh>
#include <vull/renderer/Vertex.hh>
#include <vull/support/Array.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Log.hh>
#include <vull/support/Vector.hh>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <vulkan/vulkan_core.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>

namespace {

constexpr int k_max_light_count = 6000;
constexpr int k_max_lights_per_tile = 400;
constexpr int k_tile_size = 32;

constexpr VkDeviceSize k_lights_buffer_size = k_max_light_count * sizeof(PointLight) + sizeof(glm::vec4);
constexpr VkDeviceSize k_light_visibility_size = k_max_lights_per_tile * sizeof(std::uint32_t) + sizeof(std::uint32_t);

Vector<char> load_binary(const std::string &path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    ENSURE(file);
    Vector<char> buffer(file.tellg());
    file.seekg(0);
    file.read(buffer.data(), buffer.capacity());
    return buffer;
}

VkShaderModule load_shader(const Device &device, const std::string &path) {
    auto binary = load_binary(k_shader_path + path);
    VkShaderModuleCreateInfo shader_module_ci{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = binary.size(),
        .pCode = reinterpret_cast<const std::uint32_t *>(binary.data()),
    };
    VkShaderModule shader_module = nullptr;
    ENSURE(vkCreateShaderModule(*device, &shader_module_ci, nullptr, &shader_module) == VK_SUCCESS);
    return shader_module;
}

VkPipelineShaderStageCreateInfo shader_stage_ci(VkShaderStageFlagBits stage, VkShaderModule module,
                                                const VkSpecializationInfo *specialisation_info = nullptr) {
    return {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = stage,
        .module = module,
        .pName = "main",
        .pSpecializationInfo = specialisation_info,
    };
}

} // namespace

// TODO: RenderSystem doesn't need the window, we need to store window extent in swapchain.
RenderSystem::RenderSystem(const Device &device, const Swapchain &swapchain, const Window &window,
                           const Vector<Vertex> &vertices, const Vector<std::uint32_t> &indices)
    : m_device(device), m_swapchain(swapchain), m_window(window) {
    m_row_tile_count = (window.width() + (window.width() % k_tile_size)) / k_tile_size;
    m_col_tile_count = (window.height() + (window.height() % k_tile_size)) / k_tile_size;
    create_pools();
    create_queues();
    create_render_passes();
    load_shaders();
    create_data_buffers(vertices, indices);
    create_depth_buffer();
    create_descriptors();
    create_pipeline_layouts();
    create_pipelines();
    create_output_buffers();
    allocate_command_buffers();
    create_sync_objects();
    m_lights_data = m_lights_buffer.map_memory();
    m_ubo_data = m_uniform_buffer.map_memory();
}

RenderSystem::~RenderSystem() {
    m_uniform_buffer.unmap_memory();
    m_lights_buffer.unmap_memory();
    vkDeviceWaitIdle(*m_device);
    vkDestroySemaphore(*m_device, m_main_pass_finished, nullptr);
    vkDestroySemaphore(*m_device, m_light_cull_pass_finished, nullptr);
    vkDestroySemaphore(*m_device, m_depth_pass_finished, nullptr);
    vkDestroySemaphore(*m_device, m_image_available, nullptr);
    vkDestroyFence(*m_device, m_frame_finished, nullptr);
    vkFreeCommandBuffers(*m_device, m_command_pool, m_command_buffers.size(), m_command_buffers.data());
    for (auto *output_framebuffer : m_output_framebuffers) {
        vkDestroyFramebuffer(*m_device, output_framebuffer, nullptr);
    }
    vkDestroySampler(*m_device, m_depth_sampler, nullptr);
    vkDestroyFramebuffer(*m_device, m_depth_framebuffer, nullptr);
    vkDestroyImageView(*m_device, m_depth_image_view, nullptr);
    vkDestroyPipeline(*m_device, m_main_pass_pipeline, nullptr);
    vkDestroyPipeline(*m_device, m_light_cull_pass_pipeline, nullptr);
    vkDestroyPipeline(*m_device, m_depth_pass_pipeline, nullptr);
    vkDestroyPipelineLayout(*m_device, m_main_pass_pipeline_layout, nullptr);
    vkDestroyPipelineLayout(*m_device, m_light_cull_pass_pipeline_layout, nullptr);
    vkDestroyPipelineLayout(*m_device, m_depth_pass_pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(*m_device, m_depth_sampler_set_layout, nullptr);
    vkDestroyDescriptorSetLayout(*m_device, m_ubo_set_layout, nullptr);
    vkDestroyDescriptorSetLayout(*m_device, m_lights_set_layout, nullptr);
    // TODO: Shader modules can be destroyed right after pipeline creation.
    vkDestroyShaderModule(*m_device, m_main_pass_fragment_shader, nullptr);
    vkDestroyShaderModule(*m_device, m_main_pass_vertex_shader, nullptr);
    vkDestroyShaderModule(*m_device, m_light_cull_pass_compute_shader, nullptr);
    vkDestroyShaderModule(*m_device, m_depth_pass_vertex_shader, nullptr);
    vkDestroyRenderPass(*m_device, m_main_pass_render_pass, nullptr);
    vkDestroyRenderPass(*m_device, m_depth_pass_render_pass, nullptr);
    vkDestroyDescriptorPool(*m_device, m_descriptor_pool, nullptr);
    vkDestroyCommandPool(*m_device, m_command_pool, nullptr);
}

void RenderSystem::create_pools() {
    // TODO: Don't hardcode queueFamilyIndex!
    VkCommandPoolCreateInfo command_pool_ci{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = 0,
    };
    ENSURE(vkCreateCommandPool(*m_device, &command_pool_ci, nullptr, &m_command_pool) == VK_SUCCESS);

    Array descriptor_pool_sizes{
        VkDescriptorPoolSize{
            .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 4,
        },
        VkDescriptorPoolSize{
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 3,
        },
        VkDescriptorPoolSize{
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
        },
    };
    VkDescriptorPoolCreateInfo descriptor_pool_ci{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = m_swapchain.image_views().size(),
        .poolSizeCount = descriptor_pool_sizes.size(),
        .pPoolSizes = descriptor_pool_sizes.data(),
    };
    ENSURE(vkCreateDescriptorPool(*m_device, &descriptor_pool_ci, nullptr, &m_descriptor_pool) == VK_SUCCESS);
}

void RenderSystem::create_queues() {
    for (std::uint32_t i = 0; const auto &queue_family : m_device.queue_families()) {
        const auto flags = queue_family.queueFlags;
        if ((flags & VK_QUEUE_COMPUTE_BIT) != 0u && (flags & VK_QUEUE_GRAPHICS_BIT) != 0u) {
            const std::uint32_t queue_index = 0;
            Log::trace("renderer", "Using queue %d:%d for compute", i, queue_index);
            Log::trace("renderer", "Using queue %d:%d for graphics", i, queue_index);
            vkGetDeviceQueue(*m_device, i, queue_index, &m_compute_queue);
            vkGetDeviceQueue(*m_device, i, queue_index, &m_graphics_queue);
        }
    }
    // These will still be nullptr if no queue supporting both compute and graphics is found.
    ENSURE(m_compute_queue != nullptr);
    ENSURE(m_graphics_queue != nullptr);
}

void RenderSystem::create_render_passes() {
    // Depth pass.
    {
        Array attachments{
            // Depth pass depth write -> light cull pass compute shader -> main pass depth test.
            VkAttachmentDescription{
                .format = VK_FORMAT_D32_SFLOAT,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
            },
        };
        VkAttachmentReference depth_attachment_write_ref{
            .attachment = 0,
            .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        };
        VkSubpassDescription subpass{
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .pDepthStencilAttachment = &depth_attachment_write_ref,
        };
        VkSubpassDependency subpass_dependency{
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        };
        VkRenderPassCreateInfo render_pass_ci{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .attachmentCount = attachments.size(),
            .pAttachments = attachments.data(),
            .subpassCount = 1,
            .pSubpasses = &subpass,
            .dependencyCount = 1,
            .pDependencies = &subpass_dependency,
        };
        ENSURE(vkCreateRenderPass(*m_device, &render_pass_ci, nullptr, &m_depth_pass_render_pass) == VK_SUCCESS);
    }

    // Main pass.
    {
        Array attachments{
            // Main pass fragment shader -> swapchain image (present).
            VkAttachmentDescription{
                // TODO: Don't hardcode the format, get it from surface/swapchain.
                .format = VK_FORMAT_B8G8R8A8_SRGB,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            },
            VkAttachmentDescription{
                .format = VK_FORMAT_D32_SFLOAT,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
            },
        };
        VkAttachmentReference colour_attachment_write_ref{
            .attachment = 0,
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        };
        VkAttachmentReference depth_attachment_read_ref{
            .attachment = 1,
            .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
        };
        VkSubpassDescription subpass{
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colour_attachment_write_ref,
            .pDepthStencilAttachment = &depth_attachment_read_ref,
        };
        VkSubpassDependency subpass_dependency{
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        };
        VkRenderPassCreateInfo render_pass_ci{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .attachmentCount = attachments.size(),
            .pAttachments = attachments.data(),
            .subpassCount = 1,
            .pSubpasses = &subpass,
            .dependencyCount = 1,
            .pDependencies = &subpass_dependency,
        };
        ENSURE(vkCreateRenderPass(*m_device, &render_pass_ci, nullptr, &m_main_pass_render_pass) == VK_SUCCESS);
    }
}

void RenderSystem::load_shaders() {
    // Load shaders into vulkan shader modules.
    Log::debug("renderer", "Loading shaders");
    m_depth_pass_vertex_shader = load_shader(m_device, "depth.vert.spv");
    m_light_cull_pass_compute_shader = load_shader(m_device, "light_cull.comp.spv");
    m_main_pass_vertex_shader = load_shader(m_device, "main.vert.spv");
    m_main_pass_fragment_shader = load_shader(m_device, "main.frag.spv");
}

void RenderSystem::create_data_buffers(const Vector<Vertex> &vertices, const Vector<std::uint32_t> &indices) {
    // TODO: Use transfer queue.
    // Vertex buffer.
    {
        m_vertex_buffer =
            m_device.create_buffer(vertices.size_bytes(), BufferType::VertexBuffer, MemoryUsage::CpuToGpu, false);
        void *vertex_data = m_vertex_buffer.map_memory();
        std::memcpy(vertex_data, vertices.data(), vertices.size_bytes());
        m_vertex_buffer.unmap_memory();
    }

    // Index buffer.
    {
        m_index_buffer = m_device.create_buffer(indices.size_bytes(), BufferType::IndexBuffer, MemoryUsage::CpuToGpu, false);
        void *index_data = m_index_buffer.map_memory();
        std::memcpy(index_data, indices.data(), indices.size_bytes());
        m_index_buffer.unmap_memory();
    }

    // Lights buffer.
    {
        m_lights_buffer =
            m_device.create_buffer(k_lights_buffer_size, BufferType::StorageBuffer, MemoryUsage::CpuToGpu, false);
    }

    // Light visibilities buffer.
    {
        m_light_visibilities_buffer =
            m_device.create_buffer(k_light_visibility_size * m_row_tile_count * m_col_tile_count,
                                   BufferType::StorageBuffer, MemoryUsage::GpuOnly, true);
    }

    // Uniform buffer.
    {
        m_uniform_buffer = m_device.create_buffer(sizeof(UniformBuffer), BufferType::UniformBuffer, MemoryUsage::CpuToGpu, false);
    }
}

void RenderSystem::create_depth_buffer() {
    // Create depth buffer image in fast GPU-only memory.
    VkImageCreateInfo image_ci{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        // TODO: Don't hardcode this here.
        .format = VK_FORMAT_D32_SFLOAT,
        .extent{
            .width = m_window.width(),
            .height = m_window.height(),
            .depth = 1,
        },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    m_depth_image = m_device.create_image(image_ci, MemoryUsage::GpuOnly, true);

    VkImageViewCreateInfo image_view_ci{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = *m_depth_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = image_ci.format,
        .subresourceRange{
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .levelCount = 1,
            .layerCount = 1,
        },
    };
    ENSURE(vkCreateImageView(*m_device, &image_view_ci, nullptr, &m_depth_image_view) == VK_SUCCESS);

    // Create framebuffer so that the depth buffer can be written to by the depth pass.
    Array framebuffer_attachments{
        m_depth_image_view,
    };
    VkFramebufferCreateInfo framebuffer_ci{
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = m_depth_pass_render_pass,
        .attachmentCount = framebuffer_attachments.size(),
        .pAttachments = framebuffer_attachments.data(),
        .width = m_window.width(),
        .height = m_window.height(),
        .layers = 1,
    };
    ENSURE(vkCreateFramebuffer(*m_device, &framebuffer_ci, nullptr, &m_depth_framebuffer) == VK_SUCCESS);

    // Create sampler so that the depth buffer can be sampled from the light cull pass compute shader.
    VkSamplerCreateInfo sampler_ci{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .anisotropyEnable = VK_TRUE,
        .maxAnisotropy = 16,
        .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
    };
    ENSURE(vkCreateSampler(*m_device, &sampler_ci, nullptr, &m_depth_sampler) == VK_SUCCESS);
}

void RenderSystem::create_descriptors() {
    // Lights set (Lights + LightVisibilities).
    {
        // Create set layout.
        Array layout_bindings{
            VkDescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            },
            VkDescriptorSetLayoutBinding{
                .binding = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
            },
        };
        VkDescriptorSetLayoutCreateInfo layout_ci{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = layout_bindings.size(),
            .pBindings = layout_bindings.data(),
        };
        ENSURE(vkCreateDescriptorSetLayout(*m_device, &layout_ci, nullptr, &m_lights_set_layout) == VK_SUCCESS);

        // Create set.
        VkDescriptorSetAllocateInfo allocate_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = m_descriptor_pool,
            .descriptorSetCount = 1,
            .pSetLayouts = &m_lights_set_layout,
        };
        ENSURE(vkAllocateDescriptorSets(*m_device, &allocate_info, &m_lights_set) == VK_SUCCESS);

        // Update set with buffer info.
        VkDescriptorBufferInfo lights_buffer_info{
            .buffer = *m_lights_buffer,
            .range = VK_WHOLE_SIZE,
        };
        VkDescriptorBufferInfo light_visibilities_buffer_info{
            .buffer = *m_light_visibilities_buffer,
            .range = VK_WHOLE_SIZE,
        };
        Array descriptor_writes{
            VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = m_lights_set,
                .dstBinding = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .pBufferInfo = &lights_buffer_info,
            },
            VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = m_lights_set,
                .dstBinding = 1,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .pBufferInfo = &light_visibilities_buffer_info,
            },
        };
        vkUpdateDescriptorSets(*m_device, descriptor_writes.size(), descriptor_writes.data(), 0, nullptr);
    }

    // Ubo set.
    {
        // Create set layout.
        Array layout_bindings{
            VkDescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                // NOLINTNEXTLINE
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
            },
        };
        VkDescriptorSetLayoutCreateInfo layout_ci{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = layout_bindings.size(),
            .pBindings = layout_bindings.data(),
        };
        ENSURE(vkCreateDescriptorSetLayout(*m_device, &layout_ci, nullptr, &m_ubo_set_layout) == VK_SUCCESS);

        // Create set.
        VkDescriptorSetAllocateInfo allocate_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = m_descriptor_pool,
            .descriptorSetCount = 1,
            .pSetLayouts = &m_ubo_set_layout,
        };
        ENSURE(vkAllocateDescriptorSets(*m_device, &allocate_info, &m_ubo_set) == VK_SUCCESS);

        // Update set with buffer info.
        VkDescriptorBufferInfo ubo_buffer_info{
            .buffer = *m_uniform_buffer,
            .range = VK_WHOLE_SIZE,
        };
        Array descriptor_writes{
            VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = m_ubo_set,
                .dstBinding = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pBufferInfo = &ubo_buffer_info,
            },
        };
        vkUpdateDescriptorSets(*m_device, descriptor_writes.size(), descriptor_writes.data(), 0, nullptr);
    }

    // Depth sampler set.
    {
        // Create set layout.
        Array layout_bindings{
            VkDescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            },
        };
        VkDescriptorSetLayoutCreateInfo layout_ci{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = layout_bindings.size(),
            .pBindings = layout_bindings.data(),
        };
        ENSURE(vkCreateDescriptorSetLayout(*m_device, &layout_ci, nullptr, &m_depth_sampler_set_layout) == VK_SUCCESS);

        // Create set.
        VkDescriptorSetAllocateInfo allocate_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = m_descriptor_pool,
            .descriptorSetCount = 1,
            .pSetLayouts = &m_depth_sampler_set_layout,
        };
        ENSURE(vkAllocateDescriptorSets(*m_device, &allocate_info, &m_depth_sampler_set) == VK_SUCCESS);

        // Update set with image info.
        VkDescriptorImageInfo depth_sampler_image_info{
            .sampler = m_depth_sampler,
            .imageView = m_depth_image_view,
            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
        };
        Array descriptor_writes{
            VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = m_depth_sampler_set,
                .dstBinding = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &depth_sampler_image_info,
            },
        };
        vkUpdateDescriptorSets(*m_device, descriptor_writes.size(), descriptor_writes.data(), 0, nullptr);
    }
}

void RenderSystem::create_pipeline_layouts() {
    // Depth pass.
    {
        Array set_layouts{
            m_ubo_set_layout,
        };
        VkPushConstantRange push_constant_range{
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .size = sizeof(glm::mat4),
        };
        VkPipelineLayoutCreateInfo pipeline_layout_ci{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = set_layouts.size(),
            .pSetLayouts = set_layouts.data(),
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &push_constant_range,
        };
        ENSURE(vkCreatePipelineLayout(*m_device, &pipeline_layout_ci, nullptr, &m_depth_pass_pipeline_layout) ==
               VK_SUCCESS);
    }

    // Light cull pass.
    {
        Array set_layouts{
            m_lights_set_layout,
            m_ubo_set_layout,
            m_depth_sampler_set_layout,
        };
        VkPipelineLayoutCreateInfo pipeline_layout_ci{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = set_layouts.size(),
            .pSetLayouts = set_layouts.data(),
        };
        ENSURE(vkCreatePipelineLayout(*m_device, &pipeline_layout_ci, nullptr, &m_light_cull_pass_pipeline_layout) ==
               VK_SUCCESS);
    }

    // Main pass.
    {
        Array set_layouts{
            m_lights_set_layout,
            m_ubo_set_layout,
        };
        VkPushConstantRange push_constant_range{
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .size = sizeof(glm::mat4),
        };
        VkPipelineLayoutCreateInfo pipeline_layout_ci{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = set_layouts.size(),
            .pSetLayouts = set_layouts.data(),
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &push_constant_range,
        };
        ENSURE(vkCreatePipelineLayout(*m_device, &pipeline_layout_ci, nullptr, &m_main_pass_pipeline_layout) ==
               VK_SUCCESS);
    }
}

void RenderSystem::create_pipelines() {
    // Setup shader specialisation constant information.
    struct SpecialisationData {
        std::uint32_t tile_size;
        std::uint32_t max_lights_per_tile;
        std::uint32_t tile_count;
        std::uint32_t viewport_width;
        std::uint32_t viewport_height;
    } specialisation_data{
        .tile_size = k_tile_size,
        .max_lights_per_tile = k_max_lights_per_tile,
        .tile_count = m_row_tile_count,
        .viewport_width = m_window.width(),
        .viewport_height = m_window.height(),
    };
    Array specialisation_map_entries{
        VkSpecializationMapEntry{
            .constantID = 0,
            .offset = offsetof(SpecialisationData, tile_size), // NOLINT
            .size = sizeof(SpecialisationData::tile_size),
        },
        VkSpecializationMapEntry{
            .constantID = 1,
            .offset = offsetof(SpecialisationData, max_lights_per_tile), // NOLINT
            .size = sizeof(SpecialisationData::max_lights_per_tile),
        },
        VkSpecializationMapEntry{
            .constantID = 2,
            .offset = offsetof(SpecialisationData, tile_count), // NOLINT
            .size = sizeof(SpecialisationData::tile_count),
        },
        VkSpecializationMapEntry{
            .constantID = 3,
            .offset = offsetof(SpecialisationData, viewport_width), // NOLINT
            .size = sizeof(SpecialisationData::viewport_width),
        },
        VkSpecializationMapEntry{
            .constantID = 4,
            .offset = offsetof(SpecialisationData, viewport_height), // NOLINT
            .size = sizeof(SpecialisationData::viewport_height),
        },
    };
    VkSpecializationInfo specialisation_info{
        .mapEntryCount = specialisation_map_entries.size(),
        .pMapEntries = specialisation_map_entries.data(),
        .dataSize = sizeof(SpecialisationData),
        .pData = &specialisation_data,
    };

    // Setup common pipeline state (vertex input, input assembly, etc.).
    Array<VkVertexInputAttributeDescription, 2> vertex_input_attribute_descriptions{{
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)},
    }};
    VkVertexInputBindingDescription vertex_input_binding_description{
        .binding = 0,
        .stride = sizeof(Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
    VkPipelineVertexInputStateCreateInfo vertex_input{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &vertex_input_binding_description,
        .vertexAttributeDescriptionCount = vertex_input_attribute_descriptions.size(),
        .pVertexAttributeDescriptions = vertex_input_attribute_descriptions.data(),
    };

    VkPipelineInputAssemblyStateCreateInfo input_assembly{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };

    VkRect2D scissor{
        .extent{m_window.width(), m_window.height()},
    };
    VkViewport viewport{
        .width = static_cast<float>(m_window.width()),
        .height = static_cast<float>(m_window.height()),
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

    // Depth pass.
    {
        Array shader_stages_cis{
            shader_stage_ci(VK_SHADER_STAGE_VERTEX_BIT, m_depth_pass_vertex_shader),
        };
        VkPipelineDepthStencilStateCreateInfo depth_stencil_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable = VK_TRUE,
            .depthWriteEnable = VK_TRUE,
            .depthCompareOp = VK_COMPARE_OP_LESS,
        };
        VkGraphicsPipelineCreateInfo pipeline_ci{
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = shader_stages_cis.size(),
            .pStages = shader_stages_cis.data(),
            .pVertexInputState = &vertex_input,
            .pInputAssemblyState = &input_assembly,
            .pViewportState = &viewport_state,
            .pRasterizationState = &rasterisation_state,
            .pMultisampleState = &multisample_state,
            .pDepthStencilState = &depth_stencil_state,
            .layout = m_depth_pass_pipeline_layout,
            .renderPass = m_depth_pass_render_pass,
        };
        ENSURE(vkCreateGraphicsPipelines(*m_device, nullptr, 1, &pipeline_ci, nullptr, &m_depth_pass_pipeline) ==
               VK_SUCCESS);
    }

    // Light cull pass.
    {
        VkComputePipelineCreateInfo pipeline_ci{
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage =
                shader_stage_ci(VK_SHADER_STAGE_COMPUTE_BIT, m_light_cull_pass_compute_shader, &specialisation_info),
            .layout = m_light_cull_pass_pipeline_layout,
        };
        ENSURE(vkCreateComputePipelines(*m_device, nullptr, 1, &pipeline_ci, nullptr, &m_light_cull_pass_pipeline) ==
               VK_SUCCESS);
    }

    // Main pass.
    {
        Array shader_stage_cis{
            shader_stage_ci(VK_SHADER_STAGE_VERTEX_BIT, m_main_pass_vertex_shader),
            shader_stage_ci(VK_SHADER_STAGE_FRAGMENT_BIT, m_main_pass_fragment_shader, &specialisation_info),
        };
        VkPipelineDepthStencilStateCreateInfo depth_stencil_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable = VK_TRUE,
            .depthCompareOp = VK_COMPARE_OP_EQUAL,
        };
        VkPipelineColorBlendAttachmentState blend_attachment{
            // NOLINTNEXTLINE
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                              VK_COLOR_COMPONENT_A_BIT,
        };
        VkPipelineColorBlendStateCreateInfo blend_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments = &blend_attachment,
        };
        VkGraphicsPipelineCreateInfo pipeline_ci{
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = shader_stage_cis.size(),
            .pStages = shader_stage_cis.data(),
            .pVertexInputState = &vertex_input,
            .pInputAssemblyState = &input_assembly,
            .pViewportState = &viewport_state,
            .pRasterizationState = &rasterisation_state,
            .pMultisampleState = &multisample_state,
            .pDepthStencilState = &depth_stencil_state,
            .pColorBlendState = &blend_state,
            .layout = m_main_pass_pipeline_layout,
            .renderPass = m_main_pass_render_pass,
        };
        ENSURE(vkCreateGraphicsPipelines(*m_device, nullptr, 1, &pipeline_ci, nullptr, &m_main_pass_pipeline) ==
               VK_SUCCESS);
    }
}

void RenderSystem::create_output_buffers() {
    // Create a framebuffer render target for each swapchain image. No need to create images since they're owned by the
    // swapchain. No need to create image views since Swapchain already creates them.
    m_output_framebuffers.resize(m_swapchain.image_views().size());
    for (std::uint32_t i = 0; auto *image_view : m_swapchain.image_views()) {
        // Each output render target uses the same depth buffer since we only ever render one frame at a time.
        Array framebuffer_attachments{
            image_view,
            m_depth_image_view,
        };
        VkFramebufferCreateInfo framebuffer_ci{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = m_main_pass_render_pass,
            .attachmentCount = framebuffer_attachments.size(),
            .pAttachments = framebuffer_attachments.data(),
            .width = m_window.width(),
            .height = m_window.height(),
            .layers = 1,
        };
        ENSURE(vkCreateFramebuffer(*m_device, &framebuffer_ci, nullptr, &m_output_framebuffers[i++]) == VK_SUCCESS);
    }
}

void RenderSystem::allocate_command_buffers() {
    VkCommandBufferAllocateInfo allocate_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = m_command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        // Command buffer for each main pass render target + 1 for depth pass + 1 for light cull pass.
        .commandBufferCount = m_swapchain.image_views().size() + 2,
    };
    m_command_buffers.resize(allocate_info.commandBufferCount);
    ENSURE(vkAllocateCommandBuffers(*m_device, &allocate_info, m_command_buffers.data()) == VK_SUCCESS);
}

void RenderSystem::record_command_buffers(World *world) {
    // Depth pass.
    {
        auto *cmd_buf = m_command_buffers[m_swapchain.image_views().size()];
        VkCommandBufferBeginInfo cmd_buf_bi{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        vkBeginCommandBuffer(cmd_buf, &cmd_buf_bi);
        Array<VkClearValue, 1> clear_values{};
        clear_values[0].depthStencil = {1.0f, 0};
        VkRenderPassBeginInfo render_pass_bi{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = m_depth_pass_render_pass,
            .framebuffer = m_depth_framebuffer,
            .renderArea{.extent{m_window.width(), m_window.height()}},
            .clearValueCount = clear_values.size(),
            .pClearValues = clear_values.data(),
        };
        vkCmdBeginRenderPass(cmd_buf, &render_pass_bi, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_depth_pass_pipeline);
        Array descriptor_sets{
            m_ubo_set,
        };
        vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_depth_pass_pipeline_layout, 0,
                                descriptor_sets.size(), descriptor_sets.data(), 0, nullptr);
        Array<VkDeviceSize, 1> offsets{0};
        auto *vertex_buffer = *m_vertex_buffer;
        vkCmdBindVertexBuffers(cmd_buf, 0, 1, &vertex_buffer, offsets.data());
        vkCmdBindIndexBuffer(cmd_buf, *m_index_buffer, 0, VK_INDEX_TYPE_UINT32);
        for (auto [entity, mesh, transform] : world->view<Mesh, Transform>()) {
            vkCmdPushConstants(cmd_buf, m_depth_pass_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4),
                               &transform->matrix());
            vkCmdDrawIndexed(cmd_buf, mesh->index_count(), 1, mesh->index_offset(), 0, 0);
        }
        vkCmdEndRenderPass(cmd_buf);
        vkEndCommandBuffer(cmd_buf);
    }

    // Light cull pass.
    // TODO: This doesn't need to be re-recorded every frame.
    {
        auto *cmd_buf = m_command_buffers[m_swapchain.image_views().size() + 1];
        VkCommandBufferBeginInfo cmd_buf_bi{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        vkBeginCommandBuffer(cmd_buf, &cmd_buf_bi);
        Array descriptor_sets{
            m_lights_set,
            m_ubo_set,
            m_depth_sampler_set,
        };
        vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, m_light_cull_pass_pipeline_layout, 0,
                                descriptor_sets.size(), descriptor_sets.data(), 0, nullptr);
        vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, m_light_cull_pass_pipeline);
        vkCmdDispatch(cmd_buf, m_row_tile_count, m_col_tile_count, 1);
        Array light_cull_pass_barriers{
            VkBufferMemoryBarrier{
                .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                .buffer = *m_lights_buffer,
                .size = k_lights_buffer_size,
            },
            VkBufferMemoryBarrier{
                .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                .buffer = *m_light_visibilities_buffer,
                .size = k_light_visibility_size * m_row_tile_count * m_col_tile_count,
            },
        };
        vkCmdPipelineBarrier(cmd_buf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                             nullptr, light_cull_pass_barriers.size(), light_cull_pass_barriers.data(), 0, nullptr);
        vkEndCommandBuffer(cmd_buf);
    }

    // Main pass.
    for (std::uint32_t i = 0; i < m_swapchain.image_views().size(); i++) {
        auto *cmd_buf = m_command_buffers[i];
        VkCommandBufferBeginInfo cmd_buf_bi{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        vkBeginCommandBuffer(cmd_buf, &cmd_buf_bi);
        Array<VkClearValue, 1> clear_values{};
        clear_values[0].color = {0, 0, 0, 1};
        VkRenderPassBeginInfo render_pass_bi{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = m_main_pass_render_pass,
            .framebuffer = m_output_framebuffers[i],
            .renderArea{.extent{m_window.width(), m_window.height()}},
            .clearValueCount = clear_values.size(),
            .pClearValues = clear_values.data(),
        };
        vkCmdBeginRenderPass(cmd_buf, &render_pass_bi, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_main_pass_pipeline);
        Array main_pass_descriptor_sets{
            m_lights_set,
            m_ubo_set,
        };
        vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_main_pass_pipeline_layout, 0,
                                main_pass_descriptor_sets.size(), main_pass_descriptor_sets.data(), 0, nullptr);
        Array<VkDeviceSize, 1> offsets{0};
        auto *vertex_buffer = *m_vertex_buffer;
        vkCmdBindVertexBuffers(cmd_buf, 0, 1, &vertex_buffer, offsets.data());
        vkCmdBindIndexBuffer(cmd_buf, *m_index_buffer, 0, VK_INDEX_TYPE_UINT32);
        for (auto [entity, mesh, transform] : world->view<Mesh, Transform>()) {
            vkCmdPushConstants(cmd_buf, m_main_pass_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4),
                               &transform->matrix());
            vkCmdDrawIndexed(cmd_buf, mesh->index_count(), 1, mesh->index_offset(), 0, 0);
        }
        vkCmdEndRenderPass(cmd_buf);
        vkEndCommandBuffer(cmd_buf);
    }
}

void RenderSystem::create_sync_objects() {
    VkFenceCreateInfo fence_ci{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    VkSemaphoreCreateInfo semaphore_ci{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    ENSURE(vkCreateFence(*m_device, &fence_ci, nullptr, &m_frame_finished) == VK_SUCCESS);
    ENSURE(vkCreateSemaphore(*m_device, &semaphore_ci, nullptr, &m_image_available) == VK_SUCCESS);
    ENSURE(vkCreateSemaphore(*m_device, &semaphore_ci, nullptr, &m_depth_pass_finished) == VK_SUCCESS);
    ENSURE(vkCreateSemaphore(*m_device, &semaphore_ci, nullptr, &m_light_cull_pass_finished) == VK_SUCCESS);
    ENSURE(vkCreateSemaphore(*m_device, &semaphore_ci, nullptr, &m_main_pass_finished) == VK_SUCCESS);
}

void RenderSystem::update(World *world, float) {
    // Wait for previous frame rendering to finish, and request the next swapchain image.
    vkWaitForFences(*m_device, 1, &m_frame_finished, VK_TRUE, std::numeric_limits<std::uint64_t>::max());
    vkResetFences(*m_device, 1, &m_frame_finished);
    std::uint32_t image_index = m_swapchain.acquire_next_image(m_image_available, nullptr);

    // Update dynamic buffer data.
    const std::uint32_t light_count = m_lights.size();
    std::memcpy(m_lights_data, &light_count, sizeof(std::uint32_t));
    std::memcpy(reinterpret_cast<char *>(m_lights_data) + sizeof(glm::vec4), m_lights.data(), m_lights.size_bytes());
    std::memcpy(m_ubo_data, &m_ubo, sizeof(UniformBuffer));

    // Re-record command buffers
    vkResetCommandPool(*m_device, m_command_pool, 0);
    record_command_buffers(world);

    // Execute depth pass.
    VkSubmitInfo depth_pass_si{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &m_command_buffers[m_swapchain.image_views().size()],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &m_depth_pass_finished,
    };
    vkQueueSubmit(m_graphics_queue, 1, &depth_pass_si, nullptr);

    // Execute light cull pass.
    Array light_cull_pass_wait_semaphores{
        m_depth_pass_finished,
    };
    Array<VkPipelineStageFlags, 1> light_cull_pass_wait_stages{
        // Wait for `m_depth_pass_finished` before executing the light cull pass compute shader, since we sample the
        // depth buffer there.
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    };
    VkSubmitInfo light_cull_pass_si{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = light_cull_pass_wait_semaphores.size(),
        .pWaitSemaphores = light_cull_pass_wait_semaphores.data(),
        .pWaitDstStageMask = light_cull_pass_wait_stages.data(),
        .commandBufferCount = 1,
        .pCommandBuffers = &m_command_buffers[m_swapchain.image_views().size() + 1],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &m_light_cull_pass_finished,
    };
    vkQueueSubmit(m_compute_queue, 1, &light_cull_pass_si, nullptr);

    // Execute main pass.
    Array main_pass_wait_semaphores{
        m_image_available,
        m_light_cull_pass_finished,
    };
    Array<VkPipelineStageFlags, 2> main_pass_wait_stages{
        // Wait for `m_image_available` before attempting to write to swapchain image.
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        // Wait for `m_light_cull_pass_finished` before executing the main pass fragment shader.
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    };
    VkSubmitInfo main_pass_si{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = main_pass_wait_semaphores.size(),
        .pWaitSemaphores = main_pass_wait_semaphores.data(),
        .pWaitDstStageMask = main_pass_wait_stages.data(),
        .commandBufferCount = 1,
        .pCommandBuffers = &m_command_buffers[image_index],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &m_main_pass_finished,
    };
    vkQueueSubmit(m_graphics_queue, 1, &main_pass_si, m_frame_finished);

    // Present output buffer to swapchain.
    Array present_wait_semaphores{m_main_pass_finished};
    m_swapchain.present(image_index, present_wait_semaphores);
}
