#include <Window.hh>
#include <renderer/Camera.hh>
#include <renderer/Device.hh>
#include <renderer/Instance.hh>
#include <renderer/Surface.hh>
#include <renderer/Swapchain.hh>
#include <support/Array.hh>
#include <support/Assert.hh>

#define TINYOBJLOADER_IMPLEMENTATION
#define VMA_IMPLEMENTATION
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/random.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <tiny_obj_loader.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

constexpr const char *VALIDATION_LAYER_NAME = "VK_LAYER_KHRONOS_validation";
constexpr int WIDTH = 2560;
constexpr int HEIGHT = 1440;

float g_prev_x = 0;
float g_prev_y = 0;

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
};

bool operator==(const Vertex &lhs, const Vertex &rhs) {
    return lhs.position == rhs.position && lhs.normal == rhs.normal;
}

VkDeviceQueueCreateInfo create_queue_info(std::uint32_t family_index) {
    VkDeviceQueueCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    info.queueCount = 1;
    info.queueFamilyIndex = family_index;
    float priority = 1.0F;
    info.pQueuePriorities = &priority;
    return info;
}

std::vector<char> load_binary(const std::string &path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    ENSURE(file);
    std::vector<char> buffer(file.tellg());
    file.seekg(0);
    file.read(buffer.data(), buffer.capacity());
    return std::move(buffer);
}

} // namespace

namespace std {

template <>
struct hash<Vertex> {
    std::size_t operator()(const Vertex &vertex) const {
        return hash<glm::vec3>{}(vertex.position) ^ hash<glm::vec3>{}(vertex.normal);
    }
};

} // namespace std

int main() {
    Window window(WIDTH, HEIGHT);
    glfwSetInputMode(*window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    std::uint32_t required_extension_count = 0;
    const char **required_extensions = glfwGetRequiredInstanceExtensions(&required_extension_count);
    Instance instance({required_extensions, required_extension_count});
    Device device(instance.physical_devices()[0]);
    Surface surface(instance, device, window);
    Swapchain swapchain(device, surface);

    std::optional<std::uint32_t> compute_family;
    std::optional<std::uint32_t> graphics_family;
    for (std::uint32_t i = 0; const auto &queue_family : device.queue_families()) {
        auto flags = queue_family.queueFlags;
        if ((flags & VK_QUEUE_COMPUTE_BIT) != 0U && (flags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
            compute_family = i;
            graphics_family = i;
        }
    }
    ENSURE(compute_family);
    ENSURE(graphics_family);

    VmaAllocatorCreateInfo allocator_ci{
        .physicalDevice = device.physical(),
        .device = *device,
        .instance = *instance,
    };
    VmaAllocator allocator = VK_NULL_HANDLE;
    ENSURE(vmaCreateAllocator(&allocator_ci, &allocator) == VK_SUCCESS);

    VkCommandPoolCreateInfo compute_command_pool_ci{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = *graphics_family,
    };
    VkCommandPoolCreateInfo graphics_command_pool_ci{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = *graphics_family,
    };
    VkCommandPool compute_command_pool = VK_NULL_HANDLE;
    VkCommandPool graphics_command_pool = VK_NULL_HANDLE;
    ENSURE(vkCreateCommandPool(*device, &compute_command_pool_ci, nullptr, &compute_command_pool) == VK_SUCCESS);
    ENSURE(vkCreateCommandPool(*device, &graphics_command_pool_ci, nullptr, &graphics_command_pool) == VK_SUCCESS);

    VkQueue compute_queue = VK_NULL_HANDLE;
    VkQueue graphics_queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(*device, *compute_family, 0, &compute_queue);
    vkGetDeviceQueue(*device, *graphics_family, 0, &graphics_queue);

    VkAttachmentDescription depth_attachment{
        .format = VK_FORMAT_D32_SFLOAT,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    VkAttachmentReference depth_attachment_write_ref{
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };
    VkSubpassDescription depth_pass_subpass{
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .pDepthStencilAttachment = &depth_attachment_write_ref,
    };
    VkSubpassDependency depth_pass_subpass_dependency{
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };
    VkRenderPassCreateInfo depth_pass_render_pass_ci{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &depth_attachment,
        .subpassCount = 1,
        .pSubpasses = &depth_pass_subpass,
        .dependencyCount = 1,
        .pDependencies = &depth_pass_subpass_dependency,
    };
    VkRenderPass depth_pass_render_pass = VK_NULL_HANDLE;
    ENSURE(vkCreateRenderPass(*device, &depth_pass_render_pass_ci, nullptr, &depth_pass_render_pass) == VK_SUCCESS);

    std::array<VkAttachmentDescription, 2> main_pass_attachments{
        VkAttachmentDescription{
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
            .initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
        },
    };
    VkAttachmentReference colour_attachment_ref{
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };
    VkAttachmentReference depth_attachment_ref{
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
    };
    VkSubpassDescription main_pass_subpass{
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colour_attachment_ref,
        .pDepthStencilAttachment = &depth_attachment_ref,
    };
    VkSubpassDependency main_pass_subpass_dependency{
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };
    VkRenderPassCreateInfo main_pass_render_pass_ci{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = static_cast<std::uint32_t>(main_pass_attachments.size()),
        .pAttachments = main_pass_attachments.data(),
        .subpassCount = 1,
        .pSubpasses = &main_pass_subpass,
        .dependencyCount = 1,
        .pDependencies = &main_pass_subpass_dependency,
    };
    VkRenderPass main_pass_render_pass = VK_NULL_HANDLE;
    ENSURE(vkCreateRenderPass(*device, &main_pass_render_pass_ci, nullptr, &main_pass_render_pass) == VK_SUCCESS);

    auto depth_pass_vertex_shader_code = load_binary("shaders/depth.vert.spv");
    auto light_cull_pass_compute_shader_code = load_binary("shaders/light_cull.comp.spv");
    auto main_pass_vertex_shader_code = load_binary("shaders/main.vert.spv");
    auto main_pass_fragment_shader_code = load_binary("shaders/main.frag.spv");
    VkShaderModuleCreateInfo depth_pass_vertex_shader_ci{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = depth_pass_vertex_shader_code.size(),
        .pCode = reinterpret_cast<const std::uint32_t *>(depth_pass_vertex_shader_code.data()),
    };
    VkShaderModuleCreateInfo light_cull_pass_compute_shader_ci{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = light_cull_pass_compute_shader_code.size(),
        .pCode = reinterpret_cast<const std::uint32_t *>(light_cull_pass_compute_shader_code.data()),
    };
    VkShaderModuleCreateInfo main_pass_vertex_shader_ci{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = main_pass_vertex_shader_code.size(),
        .pCode = reinterpret_cast<const std::uint32_t *>(main_pass_vertex_shader_code.data()),
    };
    VkShaderModuleCreateInfo main_pass_fragment_shader_ci{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = main_pass_fragment_shader_code.size(),
        .pCode = reinterpret_cast<const std::uint32_t *>(main_pass_fragment_shader_code.data()),
    };
    VkShaderModule depth_pass_vertex_shader = VK_NULL_HANDLE;
    VkShaderModule light_cull_pass_compute_shader = VK_NULL_HANDLE;
    VkShaderModule main_pass_vertex_shader = VK_NULL_HANDLE;
    VkShaderModule main_pass_fragment_shader = VK_NULL_HANDLE;
    ENSURE(vkCreateShaderModule(*device, &depth_pass_vertex_shader_ci, nullptr, &depth_pass_vertex_shader) ==
           VK_SUCCESS);
    ENSURE(vkCreateShaderModule(*device, &light_cull_pass_compute_shader_ci, nullptr,
                                &light_cull_pass_compute_shader) == VK_SUCCESS);
    ENSURE(vkCreateShaderModule(*device, &main_pass_vertex_shader_ci, nullptr, &main_pass_vertex_shader) == VK_SUCCESS);
    ENSURE(vkCreateShaderModule(*device, &main_pass_fragment_shader_ci, nullptr, &main_pass_fragment_shader) ==
           VK_SUCCESS);
    std::array<VkPipelineShaderStageCreateInfo, 1> depth_pass_shader_stage_cis{
        VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = depth_pass_vertex_shader,
            .pName = "main",
        },
    };
    VkPipelineShaderStageCreateInfo light_cull_pass_shader_stage_ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = light_cull_pass_compute_shader,
        .pName = "main",
    };
    std::array<VkPipelineShaderStageCreateInfo, 2> main_pass_shader_stage_cis{
        VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = main_pass_vertex_shader,
            .pName = "main",
        },
        VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = main_pass_fragment_shader,
            .pName = "main",
        },
    };

    std::array<VkVertexInputAttributeDescription, 2> attribute_descriptions{{
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)},
    }};
    VkVertexInputBindingDescription binding_description{
        .binding = 0,
        .stride = sizeof(Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
    VkPipelineVertexInputStateCreateInfo vertex_input{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &binding_description,
        .vertexAttributeDescriptionCount = attribute_descriptions.size(),
        .pVertexAttributeDescriptions = attribute_descriptions.data(),
    };

    VkPipelineInputAssemblyStateCreateInfo input_assembly{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };

    VkRect2D scissor{
        .extent{WIDTH, HEIGHT},
    };
    VkViewport viewport{
        .width = WIDTH,
        .height = HEIGHT,
        .maxDepth = 1.0F,
    };
    VkPipelineViewportStateCreateInfo viewport_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor,
    };

    VkPipelineRasterizationStateCreateInfo rasterization_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1.0F,
    };

    VkPipelineMultisampleStateCreateInfo multisample_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .minSampleShading = 1.0F,
    };

    VkPipelineDepthStencilStateCreateInfo depth_pass_depth_stencil_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
    };
    VkPipelineDepthStencilStateCreateInfo main_pass_depth_stencil_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
    };

    VkPipelineColorBlendAttachmentState main_pass_blend_attachment{
        .colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };
    VkPipelineColorBlendStateCreateInfo main_pass_blend_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &main_pass_blend_attachment,
    };

    std::array<VkDescriptorSetLayoutBinding, 2> lights_set_bindings{
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
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        },
    };
    VkDescriptorSetLayoutCreateInfo lights_set_layout_ci{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<std::uint32_t>(lights_set_bindings.size()),
        .pBindings = lights_set_bindings.data(),
    };
    VkDescriptorSetLayout lights_set_layout = VK_NULL_HANDLE;
    ENSURE(vkCreateDescriptorSetLayout(*device, &lights_set_layout_ci, nullptr, &lights_set_layout) == VK_SUCCESS);

    VkDescriptorSetLayoutBinding ubo_binding{
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_ALL,
    };
    VkDescriptorSetLayoutCreateInfo ubo_set_layout_ci{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &ubo_binding,
    };
    VkDescriptorSetLayout ubo_set_layout = VK_NULL_HANDLE;
    ENSURE(vkCreateDescriptorSetLayout(*device, &ubo_set_layout_ci, nullptr, &ubo_set_layout) == VK_SUCCESS);

    constexpr int TILE_SIZE = 32;
    int row_tile_count = (WIDTH + (WIDTH % TILE_SIZE)) / TILE_SIZE;
    int col_tile_count = (HEIGHT + (HEIGHT % TILE_SIZE)) / TILE_SIZE;
    struct PushConstantObject {
        glm::ivec2 tile_nums;
        glm::ivec2 viewport_size;
    } push_constants{
        .tile_nums = {row_tile_count, col_tile_count},
        .viewport_size = {WIDTH, HEIGHT},
    };
    VkPushConstantRange push_constant_range_compute{
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .size = sizeof(PushConstantObject),
    };
    VkPushConstantRange push_constant_range_fragment{
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        // Main pass doesn't need viewport size.
        .size = sizeof(glm::ivec2),
    };

    VkDescriptorSetLayoutBinding depth_sampler_binding{
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
    };
    VkDescriptorSetLayoutCreateInfo depth_sampler_set_layout_ci{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &depth_sampler_binding,
    };
    VkDescriptorSetLayout depth_sampler_set_layout = VK_NULL_HANDLE;
    ENSURE(vkCreateDescriptorSetLayout(*device, &depth_sampler_set_layout_ci, nullptr, &depth_sampler_set_layout) ==
           VK_SUCCESS);

    VkPipelineLayoutCreateInfo depth_pass_pipeline_layout_ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &ubo_set_layout,
    };
    VkPipelineLayout depth_pass_pipeline_layout = VK_NULL_HANDLE;
    ENSURE(vkCreatePipelineLayout(*device, &depth_pass_pipeline_layout_ci, nullptr, &depth_pass_pipeline_layout) ==
           VK_SUCCESS);

    std::array<VkDescriptorSetLayout, 3> light_cull_pass_set_layouts{
        lights_set_layout,
        ubo_set_layout,
        depth_sampler_set_layout,
    };
    VkPipelineLayoutCreateInfo light_cull_pass_pipeline_layout_ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = static_cast<std::uint32_t>(light_cull_pass_set_layouts.size()),
        .pSetLayouts = light_cull_pass_set_layouts.data(),
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_constant_range_compute,
    };
    VkPipelineLayout light_cull_pass_pipeline_layout = VK_NULL_HANDLE;
    ENSURE(vkCreatePipelineLayout(*device, &light_cull_pass_pipeline_layout_ci, nullptr,
                                  &light_cull_pass_pipeline_layout) == VK_SUCCESS);

    std::array<VkDescriptorSetLayout, 2> main_pass_set_layouts{
        lights_set_layout,
        ubo_set_layout,
    };
    VkPipelineLayoutCreateInfo main_pass_pipeline_layout_ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = static_cast<std::uint32_t>(main_pass_set_layouts.size()),
        .pSetLayouts = main_pass_set_layouts.data(),
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_constant_range_fragment,
    };
    VkPipelineLayout main_pass_pipeline_layout = VK_NULL_HANDLE;
    ENSURE(vkCreatePipelineLayout(*device, &main_pass_pipeline_layout_ci, nullptr, &main_pass_pipeline_layout) ==
           VK_SUCCESS);

    VkGraphicsPipelineCreateInfo depth_pass_pipeline_ci{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = depth_pass_shader_stage_cis.size(),
        .pStages = depth_pass_shader_stage_cis.data(),
        .pVertexInputState = &vertex_input,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterization_state,
        .pMultisampleState = &multisample_state,
        .pDepthStencilState = &depth_pass_depth_stencil_state,
        .layout = depth_pass_pipeline_layout,
        .renderPass = depth_pass_render_pass,
    };
    VkComputePipelineCreateInfo light_cull_pass_pipeline_ci{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = light_cull_pass_shader_stage_ci,
        .layout = light_cull_pass_pipeline_layout,
    };
    VkGraphicsPipelineCreateInfo main_pass_pipeline_ci{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = main_pass_shader_stage_cis.size(),
        .pStages = main_pass_shader_stage_cis.data(),
        .pVertexInputState = &vertex_input,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterization_state,
        .pMultisampleState = &multisample_state,
        .pDepthStencilState = &main_pass_depth_stencil_state,
        .pColorBlendState = &main_pass_blend_state,
        .layout = main_pass_pipeline_layout,
        .renderPass = main_pass_render_pass,
    };
    VkPipeline depth_pass_pipeline = VK_NULL_HANDLE;
    VkPipeline light_cull_pass_pipeline = VK_NULL_HANDLE;
    VkPipeline main_pass_pipeline = VK_NULL_HANDLE;
    ENSURE(vkCreateGraphicsPipelines(*device, VK_NULL_HANDLE, 1, &depth_pass_pipeline_ci, nullptr,
                                     &depth_pass_pipeline) == VK_SUCCESS);
    ENSURE(vkCreateComputePipelines(*device, VK_NULL_HANDLE, 1, &light_cull_pass_pipeline_ci, nullptr,
                                    &light_cull_pass_pipeline) == VK_SUCCESS);
    ENSURE(vkCreateGraphicsPipelines(*device, VK_NULL_HANDLE, 1, &main_pass_pipeline_ci, nullptr,
                                     &main_pass_pipeline) == VK_SUCCESS);

    VkImageCreateInfo depth_image_ci{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = main_pass_attachments[1].format,
        .extent{
            .width = WIDTH,
            .height = HEIGHT,
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
    VmaAllocationCreateInfo depth_image_allocation_ci{
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
    };
    VkImage depth_image = VK_NULL_HANDLE;
    VmaAllocation depth_image_allocation = VK_NULL_HANDLE;
    ENSURE(vmaCreateImage(allocator, &depth_image_ci, &depth_image_allocation_ci, &depth_image, &depth_image_allocation,
                          nullptr) == VK_SUCCESS);

    VkImageViewCreateInfo depth_image_view_ci{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = depth_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = main_pass_attachments[1].format,
        .subresourceRange{
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .levelCount = 1,
            .layerCount = 1,
        },
    };
    VkImageView depth_image_view = VK_NULL_HANDLE;
    ENSURE(vkCreateImageView(*device, &depth_image_view_ci, nullptr, &depth_image_view) == VK_SUCCESS);

    VkSamplerCreateInfo depth_sampler_ci{
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
    VkSampler depth_sampler = VK_NULL_HANDLE;
    ENSURE(vkCreateSampler(*device, &depth_sampler_ci, nullptr, &depth_sampler) == VK_SUCCESS);

    VkFramebufferCreateInfo depth_pass_framebuffer_ci{
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = depth_pass_render_pass,
        .attachmentCount = 1,
        .pAttachments = &depth_image_view,
        .width = WIDTH,
        .height = HEIGHT,
        .layers = 1,
    };
    VkFramebuffer depth_pass_framebuffer = VK_NULL_HANDLE;
    ENSURE(vkCreateFramebuffer(*device, &depth_pass_framebuffer_ci, nullptr, &depth_pass_framebuffer) == VK_SUCCESS);

    std::vector<VkFramebuffer> main_pass_framebuffers(swapchain.image_views().length());
    for (std::uint32_t i = 0; auto *swapchain_image_view : swapchain.image_views()) {
        std::array<VkImageView, 2> image_views{
            swapchain_image_view,
            depth_image_view,
        };
        VkFramebufferCreateInfo framebuffer_ci{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = main_pass_render_pass,
            .attachmentCount = static_cast<std::uint32_t>(image_views.size()),
            .pAttachments = image_views.data(),
            .width = WIDTH,
            .height = HEIGHT,
            .layers = 1,
        };
        ENSURE(vkCreateFramebuffer(*device, &framebuffer_ci, nullptr, &main_pass_framebuffers[i++]) == VK_SUCCESS);
    }

    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    std::unordered_map<Vertex, std::uint32_t> unique_vertices;
    tinyobj::ObjReader reader;
    ENSURE(reader.ParseFromFile("../../models/sponza.obj"));
    const auto &attrib = reader.GetAttrib();
    for (const auto &shape : reader.GetShapes()) {
        for (const auto &index : shape.mesh.indices) {
            Vertex vertex{};
            vertex.position.x = attrib.vertices[3 * index.vertex_index + 0];
            vertex.position.y = attrib.vertices[3 * index.vertex_index + 1];
            vertex.position.z = attrib.vertices[3 * index.vertex_index + 2];
            vertex.normal.x = attrib.normals[3 * index.normal_index + 0];
            vertex.normal.y = attrib.normals[3 * index.normal_index + 1];
            vertex.normal.z = attrib.normals[3 * index.normal_index + 2];
            if (!unique_vertices.contains(vertex)) {
                unique_vertices.emplace(vertex, static_cast<std::uint32_t>(vertices.size()));
                vertices.push_back(vertex);
            }
            indices.push_back(unique_vertices.at(vertex));
        }
    }

    VkBufferCreateInfo vertex_buffer_ci{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = vertices.size() * sizeof(Vertex),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    VmaAllocationCreateInfo vertex_buffer_allocation_ci{
        .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    };
    VkBuffer vertex_buffer = VK_NULL_HANDLE;
    VmaAllocation vertex_buffer_allocation = VK_NULL_HANDLE;
    ENSURE(vmaCreateBuffer(allocator, &vertex_buffer_ci, &vertex_buffer_allocation_ci, &vertex_buffer,
                           &vertex_buffer_allocation, nullptr) == VK_SUCCESS);
    void *vertex_data = nullptr;
    vmaMapMemory(allocator, vertex_buffer_allocation, &vertex_data);
    std::memcpy(vertex_data, vertices.data(), vertices.size() * sizeof(Vertex));
    vmaUnmapMemory(allocator, vertex_buffer_allocation);

    VkBufferCreateInfo index_buffer_ci{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = indices.size() * sizeof(std::uint32_t),
        .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    VmaAllocationCreateInfo index_buffer_allocation_ci{
        .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    };
    VkBuffer index_buffer = VK_NULL_HANDLE;
    VmaAllocation index_buffer_allocation = VK_NULL_HANDLE;
    ENSURE(vmaCreateBuffer(allocator, &index_buffer_ci, &index_buffer_allocation_ci, &index_buffer,
                           &index_buffer_allocation, nullptr) == VK_SUCCESS);
    void *index_data = nullptr;
    vmaMapMemory(allocator, index_buffer_allocation, &index_data);
    std::memcpy(index_data, indices.data(), indices.size() * sizeof(std::uint32_t));
    vmaUnmapMemory(allocator, index_buffer_allocation);

    struct PointLight {
        glm::vec3 position;
        float radius;
        glm::vec3 colour;
        float padding;
    };
    constexpr int MAX_LIGHT_COUNT = 6000;
    constexpr int MAX_LIGHTS_PER_TILE = 400;
    VkDeviceSize lights_buffer_size = sizeof(PointLight) * MAX_LIGHT_COUNT + sizeof(glm::vec4);
    VkBufferCreateInfo lights_buffer_ci{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = lights_buffer_size,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    VmaAllocationCreateInfo lights_buffer_allocation_ci{
        .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    };
    VkBuffer lights_buffer = VK_NULL_HANDLE;
    VmaAllocation lights_buffer_allocation = VK_NULL_HANDLE;
    ENSURE(vmaCreateBuffer(allocator, &lights_buffer_ci, &lights_buffer_allocation_ci, &lights_buffer,
                           &lights_buffer_allocation, nullptr) == VK_SUCCESS);

    VkDeviceSize light_visibility_size = (MAX_LIGHTS_PER_TILE + 1) * sizeof(std::uint32_t);
    VkDeviceSize light_visibilities_buffer_size = light_visibility_size * row_tile_count * col_tile_count;
    VkBufferCreateInfo light_visibilities_buffer_ci{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = light_visibilities_buffer_size,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    VmaAllocationCreateInfo light_visibilities_buffer_allocation_ci{
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
    };
    VkBuffer light_visibilities_buffer = VK_NULL_HANDLE;
    VmaAllocation light_visibilities_buffer_allocation = VK_NULL_HANDLE;
    ENSURE(vmaCreateBuffer(allocator, &light_visibilities_buffer_ci, &light_visibilities_buffer_allocation_ci,
                           &light_visibilities_buffer, &light_visibilities_buffer_allocation, nullptr) == VK_SUCCESS);

    struct UniformBuffer {
        glm::mat4 proj;
        glm::mat4 view;
        glm::mat4 transform;
        glm::vec3 camera_position;
    };
    VkBufferCreateInfo uniform_buffer_ci{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(UniformBuffer),
        .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    VmaAllocationCreateInfo uniform_buffer_allocation_ci{
        .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    };
    VkBuffer uniform_buffer = VK_NULL_HANDLE;
    VmaAllocation uniform_buffer_allocation = VK_NULL_HANDLE;
    ENSURE(vmaCreateBuffer(allocator, &uniform_buffer_ci, &uniform_buffer_allocation_ci, &uniform_buffer,
                           &uniform_buffer_allocation, nullptr) == VK_SUCCESS);

    std::array<VkDescriptorPoolSize, 3> descriptor_pool_sizes{
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
        .maxSets = swapchain.image_views().length(),
        .poolSizeCount = static_cast<std::uint32_t>(descriptor_pool_sizes.size()),
        .pPoolSizes = descriptor_pool_sizes.data(),
    };
    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
    ENSURE(vkCreateDescriptorPool(*device, &descriptor_pool_ci, nullptr, &descriptor_pool) == VK_SUCCESS);

    VkDescriptorSetAllocateInfo light_descriptor_ai{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &lights_set_layout,
    };
    VkDescriptorSet lights_descriptor_set = VK_NULL_HANDLE;
    ENSURE(vkAllocateDescriptorSets(*device, &light_descriptor_ai, &lights_descriptor_set) == VK_SUCCESS);
    VkDescriptorBufferInfo lights_buffer_info{
        .buffer = lights_buffer,
        .range = VK_WHOLE_SIZE,
    };
    VkDescriptorBufferInfo light_visibilities_buffer_info{
        .buffer = light_visibilities_buffer,
        .range = VK_WHOLE_SIZE,
    };
    std::array<VkWriteDescriptorSet, 2> lights_descriptor_writes{
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = lights_descriptor_set,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &lights_buffer_info,
        },
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = lights_descriptor_set,
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &light_visibilities_buffer_info,
        },
    };
    vkUpdateDescriptorSets(*device, static_cast<std::uint32_t>(lights_descriptor_writes.size()),
                           lights_descriptor_writes.data(), 0, nullptr);

    VkDescriptorSetAllocateInfo ubo_descriptor_ai{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &ubo_set_layout,
    };
    VkDescriptorSet ubo_descriptor_set = VK_NULL_HANDLE;
    ENSURE(vkAllocateDescriptorSets(*device, &ubo_descriptor_ai, &ubo_descriptor_set) == VK_SUCCESS);
    VkDescriptorBufferInfo ubo_buffer_info{
        .buffer = uniform_buffer,
        .range = VK_WHOLE_SIZE,
    };
    VkWriteDescriptorSet ubo_descriptor_write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = ubo_descriptor_set,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pBufferInfo = &ubo_buffer_info,
    };
    vkUpdateDescriptorSets(*device, 1, &ubo_descriptor_write, 0, nullptr);

    VkDescriptorSetAllocateInfo depth_sampler_descriptor_ai{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &depth_sampler_set_layout,
    };
    VkDescriptorSet depth_sampler_descriptor_set = VK_NULL_HANDLE;
    ENSURE(vkAllocateDescriptorSets(*device, &depth_sampler_descriptor_ai, &depth_sampler_descriptor_set) ==
           VK_SUCCESS);
    VkDescriptorImageInfo depth_sampler_image_info{
        .sampler = depth_sampler,
        .imageView = depth_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    VkWriteDescriptorSet depth_sampler_descriptor_write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = depth_sampler_descriptor_set,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &depth_sampler_image_info,
    };
    vkUpdateDescriptorSets(*device, 1, &depth_sampler_descriptor_write, 0, nullptr);

    VkCommandBufferAllocateInfo compute_cmd_buf_ai{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = compute_command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkCommandBufferAllocateInfo graphics_cmd_buf_ai{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = graphics_command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = swapchain.image_views().length() + 1,
    };
    VkCommandBuffer light_cull_pass_cmd_buf = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> graphics_cmd_bufs(graphics_cmd_buf_ai.commandBufferCount);
    ENSURE(vkAllocateCommandBuffers(*device, &compute_cmd_buf_ai, &light_cull_pass_cmd_buf) == VK_SUCCESS);
    ENSURE(vkAllocateCommandBuffers(*device, &graphics_cmd_buf_ai, graphics_cmd_bufs.data()) == VK_SUCCESS);
    VkCommandBuffer depth_pass_cmd_buf = graphics_cmd_bufs[0];
    auto cmd_buf_it = graphics_cmd_bufs.begin();
    std::advance(cmd_buf_it, 1);
    std::span<VkCommandBuffer> main_pass_cmd_bufs(cmd_buf_it, graphics_cmd_bufs.end());

    VkCommandBufferBeginInfo depth_pass_cmd_buf_bi{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    };
    ENSURE(vkBeginCommandBuffer(depth_pass_cmd_buf, &depth_pass_cmd_buf_bi) == VK_SUCCESS);
    std::array<VkClearValue, 1> depth_pass_clear_values{};
    depth_pass_clear_values[0].depthStencil = {1, 0};
    VkRenderPassBeginInfo depth_pass_render_pass_bi{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = depth_pass_render_pass,
        .framebuffer = depth_pass_framebuffer,
        .renderArea{.extent{WIDTH, HEIGHT}},
        .clearValueCount = static_cast<std::uint32_t>(depth_pass_clear_values.size()),
        .pClearValues = depth_pass_clear_values.data(),
    };
    std::array<VkDeviceSize, 1> offsets{0};
    vkCmdBeginRenderPass(depth_pass_cmd_buf, &depth_pass_render_pass_bi, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(depth_pass_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, depth_pass_pipeline);
    vkCmdBindDescriptorSets(depth_pass_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, depth_pass_pipeline_layout, 0, 1,
                            &ubo_descriptor_set, 0, nullptr);
    vkCmdBindVertexBuffers(depth_pass_cmd_buf, 0, 1, &vertex_buffer, offsets.data());
    vkCmdBindIndexBuffer(depth_pass_cmd_buf, index_buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(depth_pass_cmd_buf, static_cast<std::uint32_t>(indices.size()), 1, 0, 0, 0);
    vkCmdEndRenderPass(depth_pass_cmd_buf);
    ENSURE(vkEndCommandBuffer(depth_pass_cmd_buf) == VK_SUCCESS);

    VkCommandBufferBeginInfo light_cull_pass_cmd_buf_bi{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    };
    ENSURE(vkBeginCommandBuffer(light_cull_pass_cmd_buf, &light_cull_pass_cmd_buf_bi) == VK_SUCCESS);
    std::array<VkBufferMemoryBarrier, 2> light_cull_pass_barriers{
        VkBufferMemoryBarrier{
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .buffer = lights_buffer,
            .size = lights_buffer_size,
        },
        VkBufferMemoryBarrier{
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .buffer = light_visibilities_buffer,
            .size = light_visibilities_buffer_size,
        },
    };
    vkCmdPipelineBarrier(light_cull_pass_cmd_buf, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr,
                         static_cast<std::uint32_t>(light_cull_pass_barriers.size()), light_cull_pass_barriers.data(),
                         0, nullptr);
    std::array<VkDescriptorSet, 3> light_cull_pass_descriptor_sets{
        lights_descriptor_set,
        ubo_descriptor_set,
        depth_sampler_descriptor_set,
    };
    vkCmdBindDescriptorSets(light_cull_pass_cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, light_cull_pass_pipeline_layout, 0,
                            static_cast<std::uint32_t>(light_cull_pass_descriptor_sets.size()),
                            light_cull_pass_descriptor_sets.data(), 0, nullptr);
    vkCmdPushConstants(light_cull_pass_cmd_buf, light_cull_pass_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(PushConstantObject), &push_constants);
    vkCmdBindPipeline(light_cull_pass_cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, light_cull_pass_pipeline);
    vkCmdDispatch(light_cull_pass_cmd_buf, row_tile_count, col_tile_count, 1);
    light_cull_pass_barriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    light_cull_pass_barriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    light_cull_pass_barriers[1].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    light_cull_pass_barriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(light_cull_pass_cmd_buf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         static_cast<std::uint32_t>(light_cull_pass_barriers.size()), light_cull_pass_barriers.data(),
                         0, nullptr);
    ENSURE(vkEndCommandBuffer(light_cull_pass_cmd_buf) == VK_SUCCESS);

    for (int i = 0; auto *main_pass_cmd_buf : main_pass_cmd_bufs) {
        VkCommandBufferBeginInfo main_pass_cmd_buf_bi{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        };
        ENSURE(vkBeginCommandBuffer(main_pass_cmd_buf, &main_pass_cmd_buf_bi) == VK_SUCCESS);
        std::array<VkClearValue, 1> main_pass_clear_values{};
        main_pass_clear_values[0].color = {0, 0, 0, 1};
        VkRenderPassBeginInfo main_pass_render_pass_bi{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = main_pass_render_pass,
            .framebuffer = main_pass_framebuffers[i++],
            .renderArea{.extent{WIDTH, HEIGHT}},
            .clearValueCount = static_cast<std::uint32_t>(main_pass_clear_values.size()),
            .pClearValues = main_pass_clear_values.data(),
        };
        vkCmdBeginRenderPass(main_pass_cmd_buf, &main_pass_render_pass_bi, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdPushConstants(main_pass_cmd_buf, main_pass_pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(glm::ivec2), &push_constants);
        vkCmdBindPipeline(main_pass_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, main_pass_pipeline);
        std::array<VkDescriptorSet, 2> main_pass_descriptor_sets{
            lights_descriptor_set,
            ubo_descriptor_set,
        };
        vkCmdBindDescriptorSets(main_pass_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, main_pass_pipeline_layout, 0,
                                static_cast<std::uint32_t>(main_pass_descriptor_sets.size()),
                                main_pass_descriptor_sets.data(), 0, nullptr);
        vkCmdBindVertexBuffers(main_pass_cmd_buf, 0, 1, &vertex_buffer, offsets.data());
        vkCmdBindIndexBuffer(main_pass_cmd_buf, index_buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(main_pass_cmd_buf, static_cast<std::uint32_t>(indices.size()), 1, 0, 0, 0);
        vkCmdEndRenderPass(main_pass_cmd_buf);
        ENSURE(vkEndCommandBuffer(main_pass_cmd_buf) == VK_SUCCESS);
    }

    VkFenceCreateInfo fence_ci{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    VkFence fence = VK_NULL_HANDLE;
    ENSURE(vkCreateFence(*device, &fence_ci, nullptr, &fence) == VK_SUCCESS);

    VkSemaphoreCreateInfo semaphore_ci{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    VkSemaphore image_available = VK_NULL_HANDLE;
    VkSemaphore depth_pass_finished = VK_NULL_HANDLE;
    VkSemaphore light_cull_pass_finished = VK_NULL_HANDLE;
    VkSemaphore main_pass_finished = VK_NULL_HANDLE;
    ENSURE(vkCreateSemaphore(*device, &semaphore_ci, nullptr, &image_available) == VK_SUCCESS);
    ENSURE(vkCreateSemaphore(*device, &semaphore_ci, nullptr, &depth_pass_finished) == VK_SUCCESS);
    ENSURE(vkCreateSemaphore(*device, &semaphore_ci, nullptr, &light_cull_pass_finished) == VK_SUCCESS);
    ENSURE(vkCreateSemaphore(*device, &semaphore_ci, nullptr, &main_pass_finished) == VK_SUCCESS);

    std::vector<PointLight> lights(3000);
    std::array<glm::vec3, 3000> dsts{};
    std::array<glm::vec3, 3000> srcs{};
    for (int i = 0; auto &light : lights) {
        light.colour = glm::linearRand(glm::vec3(0.1F), glm::vec3(0.5F));
        light.radius = glm::linearRand(15.0F, 30.0F);
        light.position.x = glm::linearRand(-183, 188);
        light.position.y = glm::linearRand(-106, 116);
        light.position.z = glm::linearRand(-10, 142);
        dsts[i] = light.position;
        auto rand = glm::linearRand(30, 60);
        switch (glm::linearRand(0, 5)) {
        case 0:
            dsts[i].x += rand;
            break;
        case 1:
            dsts[i].y += rand;
            break;
        case 2:
            dsts[i].z += rand;
            break;
        case 3:
            dsts[i].x -= rand;
            break;
        case 4:
            dsts[i].y -= rand;
            break;
        case 5:
            dsts[i].z -= rand;
            break;
        }
        srcs[i++] = light.position;
    }
    UniformBuffer ubo{
        .proj = glm::perspective(glm::radians(45.0F), window.aspect_ratio(), 0.1F, 1000.0F),
        .transform = glm::scale(glm::mat4(1.0F), glm::vec3(0.1F)),
    };
    ubo.proj[1][1] *= -1;

    Camera camera(glm::vec3(24.0F, 0.2F, 24.4F));
    glfwSetWindowUserPointer(*window, &camera);
    glfwSetCursorPosCallback(*window, [](GLFWwindow *window, double xpos, double ypos) {
        auto *camera = static_cast<Camera *>(glfwGetWindowUserPointer(window));
        auto x = static_cast<float>(xpos);
        auto y = static_cast<float>(ypos);
        camera->handle_mouse_movement(x - g_prev_x, -(y - g_prev_y));
        g_prev_x = x;
        g_prev_y = y;
    });

    void *lights_data = nullptr;
    vmaMapMemory(allocator, lights_buffer_allocation, &lights_data);
    void *ubo_data = nullptr;
    vmaMapMemory(allocator, uniform_buffer_allocation, &ubo_data);

    double previous_time = glfwGetTime();
    double fps_counter_prev_time = glfwGetTime();
    int frame_count = 0;
    while (!window.should_close()) {
        double current_time = glfwGetTime();
        auto dt = static_cast<float>(current_time - previous_time);
        previous_time = current_time;
        frame_count++;
        if (current_time - fps_counter_prev_time >= 1.0) {
            std::cout << "FPS: " << frame_count << '\n';
            frame_count = 0;
            fps_counter_prev_time = current_time;
        }

        vkWaitForFences(*device, 1, &fence, VK_TRUE, std::numeric_limits<std::uint64_t>::max());
        vkResetFences(*device, 1, &fence);
        std::uint32_t image_index = swapchain.acquire_next_image(image_available, nullptr);

        ubo.view = camera.view_matrix();
        ubo.camera_position = camera.position();
        camera.update(window);
        for (int i = 0; auto &light : lights) {
            light.position = glm::mix(light.position, dsts[i], dt);
            if (glm::distance(light.position, dsts[i]) <= 6.0F) {
                std::swap(dsts[i], srcs[i]);
            }
            i++;
        }

        int light_count = lights.size();
        std::memcpy(lights_data, &light_count, sizeof(std::uint32_t));
        std::memcpy((char *)lights_data + sizeof(glm::vec4), lights.data(), lights.size() * sizeof(PointLight));
        std::memcpy(ubo_data, &ubo, sizeof(UniformBuffer));

        VkSubmitInfo depth_pass_si{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &depth_pass_cmd_buf,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &depth_pass_finished,
        };
        vkQueueSubmit(graphics_queue, 1, &depth_pass_si, VK_NULL_HANDLE);

        std::array<VkPipelineStageFlags, 1> light_cull_pass_wait_stages{
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        };
        VkSubmitInfo light_cull_pass_si{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &depth_pass_finished,
            .pWaitDstStageMask = light_cull_pass_wait_stages.data(),
            .commandBufferCount = 1,
            .pCommandBuffers = &light_cull_pass_cmd_buf,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &light_cull_pass_finished,
        };
        vkQueueSubmit(compute_queue, 1, &light_cull_pass_si, VK_NULL_HANDLE);

        std::array<VkSemaphore, 2> main_pass_wait_semaphores{
            image_available,
            light_cull_pass_finished,
        };
        std::array<VkPipelineStageFlags, 2> main_pass_wait_stages{
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        };
        VkSubmitInfo main_pass_si{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = static_cast<std::uint32_t>(main_pass_wait_semaphores.size()),
            .pWaitSemaphores = main_pass_wait_semaphores.data(),
            .pWaitDstStageMask = main_pass_wait_stages.data(),
            .commandBufferCount = 1,
            .pCommandBuffers = &main_pass_cmd_bufs[image_index],
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &main_pass_finished,
        };
        vkQueueSubmit(graphics_queue, 1, &main_pass_si, fence);

        Array present_wait_semaphores{main_pass_finished};
        swapchain.present(image_index, present_wait_semaphores);
        Window::poll_events();
    }
    vmaUnmapMemory(allocator, lights_buffer_allocation);
    vmaUnmapMemory(allocator, uniform_buffer_allocation);

    vkDeviceWaitIdle(*device);
    vkDestroySemaphore(*device, main_pass_finished, nullptr);
    vkDestroySemaphore(*device, light_cull_pass_finished, nullptr);
    vkDestroySemaphore(*device, depth_pass_finished, nullptr);
    vkDestroySemaphore(*device, image_available, nullptr);
    vkDestroyFence(*device, fence, nullptr);
    vkFreeCommandBuffers(*device, graphics_command_pool, graphics_cmd_bufs.size(), graphics_cmd_bufs.data());
    vkFreeCommandBuffers(*device, compute_command_pool, 1, &light_cull_pass_cmd_buf);
    vkDestroyDescriptorPool(*device, descriptor_pool, nullptr);
    vmaDestroyBuffer(allocator, uniform_buffer, uniform_buffer_allocation);
    vmaDestroyBuffer(allocator, light_visibilities_buffer, light_visibilities_buffer_allocation);
    vmaDestroyBuffer(allocator, lights_buffer, lights_buffer_allocation);
    vmaDestroyBuffer(allocator, index_buffer, index_buffer_allocation);
    vmaDestroyBuffer(allocator, vertex_buffer, vertex_buffer_allocation);
    for (auto *main_pass_framebuffer : main_pass_framebuffers) {
        vkDestroyFramebuffer(*device, main_pass_framebuffer, nullptr);
    }
    vkDestroyFramebuffer(*device, depth_pass_framebuffer, nullptr);
    vkDestroySampler(*device, depth_sampler, nullptr);
    vkDestroyImageView(*device, depth_image_view, nullptr);
    vmaDestroyImage(allocator, depth_image, depth_image_allocation);
    vkDestroyPipeline(*device, main_pass_pipeline, nullptr);
    vkDestroyPipeline(*device, light_cull_pass_pipeline, nullptr);
    vkDestroyPipeline(*device, depth_pass_pipeline, nullptr);
    vkDestroyPipelineLayout(*device, main_pass_pipeline_layout, nullptr);
    vkDestroyPipelineLayout(*device, light_cull_pass_pipeline_layout, nullptr);
    vkDestroyPipelineLayout(*device, depth_pass_pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(*device, depth_sampler_set_layout, nullptr);
    vkDestroyDescriptorSetLayout(*device, ubo_set_layout, nullptr);
    vkDestroyDescriptorSetLayout(*device, lights_set_layout, nullptr);
    vkDestroyShaderModule(*device, main_pass_fragment_shader, nullptr);
    vkDestroyShaderModule(*device, main_pass_vertex_shader, nullptr);
    vkDestroyShaderModule(*device, light_cull_pass_compute_shader, nullptr);
    vkDestroyShaderModule(*device, depth_pass_vertex_shader, nullptr);
    vkDestroyRenderPass(*device, main_pass_render_pass, nullptr);
    vkDestroyRenderPass(*device, depth_pass_render_pass, nullptr);
    vkDestroyCommandPool(*device, graphics_command_pool, nullptr);
    vkDestroyCommandPool(*device, compute_command_pool, nullptr);
    vmaDestroyAllocator(allocator);
}
