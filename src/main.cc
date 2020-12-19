#include <Window.hh>
#include <support/Assert.hh>

#define VMA_IMPLEMENTATION
#include <GLFW/glfw3.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <fmt/core.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace {

constexpr const char *VALIDATION_LAYER_NAME = "VK_LAYER_KHRONOS_validation";

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

int main() {
    Window window(800, 600);

    VkApplicationInfo application_info{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "vull",
        .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
        .pEngineName = "vull-engine",
        .engineVersion = VK_MAKE_VERSION(0, 1, 0),
        .apiVersion = VK_API_VERSION_1_2,
    };
    std::uint32_t required_extension_count = 0;
    const char **required_extensions = glfwGetRequiredInstanceExtensions(&required_extension_count);
    VkInstanceCreateInfo instance_ci{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &application_info,
        .enabledExtensionCount = required_extension_count,
        .ppEnabledExtensionNames = required_extensions,
    };

#ifndef NDEBUG
    std::uint32_t layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    std::vector<VkLayerProperties> layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, layers.data());

    auto it = std::find_if(layers.begin(), layers.end(), [](const auto &layer) {
        return strcmp(layer.layerName, VALIDATION_LAYER_NAME) == 0;
    });
    ENSURE(it != layers.end());
    const char *layer_name = it->layerName;
    instance_ci.enabledLayerCount = 1;
    instance_ci.ppEnabledLayerNames = &layer_name;
#endif
    VkInstance instance = VK_NULL_HANDLE;
    ENSURE(vkCreateInstance(&instance_ci, nullptr, &instance) == VK_SUCCESS);

    std::uint32_t phys_device_count = 0;
    vkEnumeratePhysicalDevices(instance, &phys_device_count, nullptr);
    std::vector<VkPhysicalDevice> phys_devices(phys_device_count);
    vkEnumeratePhysicalDevices(instance, &phys_device_count, phys_devices.data());
    ENSURE(!phys_devices.empty());
    VkPhysicalDevice phys_device = phys_devices[0];

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    ENSURE(glfwCreateWindowSurface(instance, *window, nullptr, &surface) == VK_SUCCESS);

    std::uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(phys_device, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(phys_device, &queue_family_count, queue_families.data());
    std::optional<std::uint32_t> compute_family;
    std::optional<std::uint32_t> graphics_family;
    std::optional<std::uint32_t> present_family;
    for (std::uint32_t i = 0; i < queue_family_count; i++) {
        auto flags = queue_families[i].queueFlags;
        if ((flags & VK_QUEUE_COMPUTE_BIT) != 0U) {
            compute_family = i;
        }
        if ((flags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
            graphics_family = i;
        }
        VkBool32 present_supported = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(phys_device, i, surface, &present_supported);
        if (present_supported == VK_TRUE) {
            present_family = i;
        }
    }
    ENSURE(compute_family);
    ENSURE(graphics_family);
    ENSURE(present_family);

    std::vector<VkDeviceQueueCreateInfo> queue_cis;
    if (*compute_family != *present_family) {
        queue_cis.push_back(create_queue_info(*compute_family));
    }
    queue_cis.push_back(create_queue_info(*graphics_family));
    queue_cis.push_back(create_queue_info(*present_family));

    std::array<const char *, 1> extensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkPhysicalDeviceFeatures device_features{};
    VkDeviceCreateInfo device_ci{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = static_cast<std::uint32_t>(queue_cis.size()),
        .pQueueCreateInfos = queue_cis.data(),
        .enabledExtensionCount = static_cast<std::uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data(),
        .pEnabledFeatures = &device_features,
    };
    VkDevice device = VK_NULL_HANDLE;
    ENSURE(vkCreateDevice(phys_device, &device_ci, nullptr, &device) == VK_SUCCESS);

    VkSurfaceCapabilitiesKHR surface_capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys_device, surface, &surface_capabilities);
    std::vector<std::uint32_t> queue_family_indices;
    if (*compute_family != *present_family) {
        queue_family_indices.push_back(*compute_family);
    }
    queue_family_indices.push_back(*graphics_family);
    queue_family_indices.push_back(*present_family);
    VkSurfaceFormatKHR surface_format{
        .format = VK_FORMAT_B8G8R8A8_SRGB,
        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
    };
    VkSwapchainCreateInfoKHR swapchain_ci{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = surface_capabilities.minImageCount + 1,
        .imageFormat = surface_format.format,
        .imageColorSpace = surface_format.colorSpace,
        .imageExtent = {800, 600},
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_CONCURRENT,
        .queueFamilyIndexCount = static_cast<std::uint32_t>(queue_family_indices.size()),
        .pQueueFamilyIndices = queue_family_indices.data(),
        .preTransform = surface_capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE,
    };
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    ENSURE(vkCreateSwapchainKHR(device, &swapchain_ci, nullptr, &swapchain) == VK_SUCCESS);

    // TODO: Already known?
    std::uint32_t swapchain_image_count = 0;
    vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count, nullptr);
    std::vector<VkImage> swapchain_images(swapchain_image_count);
    vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count, swapchain_images.data());
    std::vector<VkImageView> swapchain_image_views(swapchain_image_count);
    for (std::uint32_t i = 0; i < swapchain_image_count; i++) {
        VkImageViewCreateInfo image_view_ci{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = swapchain_images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = surface_format.format,
            .components{
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY,
            },
            .subresourceRange{
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        ENSURE(vkCreateImageView(device, &image_view_ci, nullptr, &swapchain_image_views[i]) == VK_SUCCESS);
    }

    struct Vertex {
        glm::vec3 position;
    };
    std::array<VkVertexInputAttributeDescription, 1> attribute_descriptions{
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)},
    };
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
        .extent{800, 600},
    };
    VkViewport viewport{
        .width = 800,
        .height = 600,
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
        .cullMode = VK_CULL_MODE_NONE, // TODO: Enable.
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .lineWidth = 1.0F,
    };

    VkPipelineMultisampleStateCreateInfo multisample_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .minSampleShading = 1.0F,
    };

    VkPipelineColorBlendAttachmentState blend_attachment{
        .colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };
    VkPipelineColorBlendStateCreateInfo blend_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &blend_attachment,
    };

    VkDescriptorSetLayoutBinding ubo_binding{
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
    };
    VkDescriptorSetLayoutCreateInfo ubo_layout_ci{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &ubo_binding,
    };
    VkDescriptorSetLayout ubo_layout = VK_NULL_HANDLE;
    ENSURE(vkCreateDescriptorSetLayout(device, &ubo_layout_ci, nullptr, &ubo_layout) == VK_SUCCESS);

    VkPipelineLayoutCreateInfo pipeline_layout_ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &ubo_layout,
    };
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    ENSURE(vkCreatePipelineLayout(device, &pipeline_layout_ci, nullptr, &pipeline_layout) == VK_SUCCESS);

    VkAttachmentDescription colour_attachment{
        .format = VK_FORMAT_B8G8R8A8_SRGB,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };
    VkAttachmentReference colour_attachment_ref{
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };
    VkSubpassDescription subpass_description{
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colour_attachment_ref,
    };
    VkSubpassDependency subpass_dependency{
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };
    VkRenderPassCreateInfo render_pass_ci{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &colour_attachment,
        .subpassCount = 1,
        .pSubpasses = &subpass_description,
        .dependencyCount = 1,
        .pDependencies = &subpass_dependency,
    };
    VkRenderPass render_pass = VK_NULL_HANDLE;
    ENSURE(vkCreateRenderPass(device, &render_pass_ci, nullptr, &render_pass) == VK_SUCCESS);

    auto vertex_shader_code = load_binary("shaders/main.vert.spv");
    auto fragment_shader_code = load_binary("shaders/main.frag.spv");
    VkShaderModuleCreateInfo vertex_shader_ci{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = vertex_shader_code.size(),
        .pCode = reinterpret_cast<const std::uint32_t *>(vertex_shader_code.data()),
    };
    VkShaderModuleCreateInfo fragment_shader_ci{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = fragment_shader_code.size(),
        .pCode = reinterpret_cast<const std::uint32_t *>(fragment_shader_code.data()),
    };
    VkShaderModule vertex_shader = VK_NULL_HANDLE;
    VkShaderModule fragment_shader = VK_NULL_HANDLE;
    ENSURE(vkCreateShaderModule(device, &vertex_shader_ci, nullptr, &vertex_shader) == VK_SUCCESS);
    ENSURE(vkCreateShaderModule(device, &fragment_shader_ci, nullptr, &fragment_shader) == VK_SUCCESS);

    std::array<VkPipelineShaderStageCreateInfo, 2> shader_stage_cis{
        VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertex_shader,
            .pName = "main",
        },
        VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragment_shader,
            .pName = "main",
        },
    };
    VkGraphicsPipelineCreateInfo pipeline_ci{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = shader_stage_cis.size(),
        .pStages = shader_stage_cis.data(),
        .pVertexInputState = &vertex_input,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterization_state,
        .pMultisampleState = &multisample_state,
        .pColorBlendState = &blend_state,
        .layout = pipeline_layout,
        .renderPass = render_pass,
    };
    VkPipeline pipeline = VK_NULL_HANDLE;
    ENSURE(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_ci, nullptr, &pipeline) == VK_SUCCESS);

    std::vector<VkFramebuffer> framebuffers(swapchain_image_views.size());
    for (std::uint32_t i = 0; auto *image_view : swapchain_image_views) {
        VkFramebufferCreateInfo framebuffer_ci{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = render_pass,
            .attachmentCount = 1,
            .pAttachments = &image_view,
            .width = 800,
            .height = 600,
            .layers = 1,
        };
        ENSURE(vkCreateFramebuffer(device, &framebuffer_ci, nullptr, &framebuffers[i++]) == VK_SUCCESS);
    }

    VmaAllocatorCreateInfo allocator_ci{
        .physicalDevice = phys_device,
        .device = device,
        .instance = instance,
    };
    VmaAllocator allocator = VK_NULL_HANDLE;
    ENSURE(vmaCreateAllocator(&allocator_ci, &allocator) == VK_SUCCESS);

    std::vector<Vertex> vertices;
    std::vector<std::uint16_t> indices;
    Assimp::Importer importer;
    const auto *scene = importer.ReadFile("../../models/suzanne.obj", aiProcess_Triangulate | aiProcess_FlipUVs);
    ENSURE(scene && !(scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) && scene->mRootNode);
    const auto *root = scene->mRootNode;
    for (auto i = 0; i < root->mNumChildren; i++) {
        const auto *node = root->mChildren[i];
        ENSURE(node->mNumChildren == 0);
        if (node->mNumMeshes == 0) {
            continue;
        }
        ENSURE(node->mNumMeshes == 1);
        const auto *mesh = scene->mMeshes[node->mMeshes[0]];
        for (auto j = 0; j < mesh->mNumVertices; j++) {
            auto &vertex = vertices.emplace_back();
            vertex.position.x = mesh->mVertices[j].x;
            vertex.position.y = mesh->mVertices[j].y;
            vertex.position.z = mesh->mVertices[j].z;
        }
        for (auto j = 0; j < mesh->mNumFaces; j++) {
            auto face = mesh->mFaces[j];
            for (auto k = 0; k < face.mNumIndices; k++) {
                indices.push_back(face.mIndices[k]);
            }
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
        .size = indices.size() * sizeof(std::uint16_t),
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
    std::memcpy(index_data, indices.data(), indices.size() * sizeof(std::uint16_t));
    vmaUnmapMemory(allocator, index_buffer_allocation);

    struct UniformBuffer {
        alignas(16) glm::mat4 proj;
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 transform;
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
    std::vector<VkBuffer> uniform_buffers(swapchain_image_views.size());
    std::vector<VmaAllocation> uniform_buffer_allocations(swapchain_image_views.size());
    for (std::size_t i = 0; i < swapchain_image_views.size(); i++) {
        ENSURE(vmaCreateBuffer(allocator, &uniform_buffer_ci, &uniform_buffer_allocation_ci, &uniform_buffers[i],
                               &uniform_buffer_allocations[i], nullptr) == VK_SUCCESS);
    }

    VkDescriptorPoolSize descriptor_pool_size{
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = static_cast<std::uint32_t>(swapchain_image_views.size()),
    };
    VkDescriptorPoolCreateInfo descriptor_pool_ci{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = static_cast<std::uint32_t>(swapchain_image_views.size()),
        .poolSizeCount = 1,
        .pPoolSizes = &descriptor_pool_size,
    };
    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
    ENSURE(vkCreateDescriptorPool(device, &descriptor_pool_ci, nullptr, &descriptor_pool) == VK_SUCCESS);

    std::vector<VkDescriptorSetLayout> ubo_layouts(swapchain_image_views.size(), ubo_layout);
    VkDescriptorSetAllocateInfo ubo_descriptors_ai{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptor_pool,
        .descriptorSetCount = static_cast<std::uint32_t>(swapchain_image_views.size()),
        .pSetLayouts = ubo_layouts.data(),
    };
    std::vector<VkDescriptorSet> ubo_descriptors(swapchain_image_views.size());
    ENSURE(vkAllocateDescriptorSets(device, &ubo_descriptors_ai, ubo_descriptors.data()) == VK_SUCCESS);

    for (std::size_t i = 0; auto *uniform_buffer : uniform_buffers) {
        VkDescriptorBufferInfo buffer_info{
            .buffer = uniform_buffer,
            .range = VK_WHOLE_SIZE,
        };
        VkWriteDescriptorSet descriptor_write{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = ubo_descriptors[i++],
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &buffer_info,
        };
        vkUpdateDescriptorSets(device, 1, &descriptor_write, 0, nullptr);
    }

    VkCommandPoolCreateInfo command_pool_ci{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = *graphics_family,
    };
    VkCommandPool command_pool = VK_NULL_HANDLE;
    ENSURE(vkCreateCommandPool(device, &command_pool_ci, nullptr, &command_pool) == VK_SUCCESS);

    VkCommandBufferAllocateInfo command_buffer_ai{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = static_cast<std::uint32_t>(framebuffers.size()),
    };
    std::vector<VkCommandBuffer> command_buffers(command_buffer_ai.commandBufferCount);
    ENSURE(vkAllocateCommandBuffers(device, &command_buffer_ai, command_buffers.data()) == VK_SUCCESS);

    for (std::size_t i = 0; auto *cmd_buf : command_buffers) {
        VkCommandBufferBeginInfo cmd_buf_bi{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        };
        ENSURE(vkBeginCommandBuffer(cmd_buf, &cmd_buf_bi) == VK_SUCCESS);
        VkClearValue clear_colour{0, 0, 0, 1};
        VkRenderPassBeginInfo render_pass_bi{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = render_pass,
            .framebuffer = framebuffers[i],
            .renderArea{.extent{800, 600}},
            .clearValueCount = 1,
            .pClearValues = &clear_colour,
        };
        std::array<VkDeviceSize, 1> offsets{0};
        vkCmdBeginRenderPass(cmd_buf, &render_pass_bi, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdBindVertexBuffers(cmd_buf, 0, 1, &vertex_buffer, offsets.data());
        vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &ubo_descriptors[i++],
                                0, nullptr);
        vkCmdBindIndexBuffer(cmd_buf, index_buffer, 0, VK_INDEX_TYPE_UINT16);
        vkCmdDrawIndexed(cmd_buf, static_cast<std::uint32_t>(indices.size()), 1, 0, 0, 0);
        vkCmdEndRenderPass(cmd_buf);
        ENSURE(vkEndCommandBuffer(cmd_buf) == VK_SUCCESS);
    }

    VkFenceCreateInfo fence_ci{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    VkFence fence = VK_NULL_HANDLE;
    ENSURE(vkCreateFence(device, &fence_ci, nullptr, &fence) == VK_SUCCESS);

    VkSemaphoreCreateInfo semaphore_ci{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    VkSemaphore image_available = VK_NULL_HANDLE;
    VkSemaphore rendering_finished = VK_NULL_HANDLE;
    ENSURE(vkCreateSemaphore(device, &semaphore_ci, nullptr, &image_available) == VK_SUCCESS);
    ENSURE(vkCreateSemaphore(device, &semaphore_ci, nullptr, &rendering_finished) == VK_SUCCESS);

    VkQueue graphics_queue = VK_NULL_HANDLE;
    VkQueue present_queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(device, *graphics_family, 0, &graphics_queue);
    vkGetDeviceQueue(device, *present_family, 0, &present_queue);
    float time = 0.0F;
    while (!window.should_close()) {
        Window::poll_events();
        vkWaitForFences(device, 1, &fence, VK_TRUE, std::numeric_limits<std::uint64_t>::max());
        vkResetFences(device, 1, &fence);
        std::uint32_t image_index = 0;
        vkAcquireNextImageKHR(device, swapchain, std::numeric_limits<std::uint64_t>::max(), image_available,
                              VK_NULL_HANDLE, &image_index);
        UniformBuffer ubo{
            .proj = glm::perspective(glm::radians(45.0F), window.aspect_ratio(), 0.1F, 1000.0F),
            .view =
                glm::lookAt(glm::vec3(50.0F, 0.0F, 0.0F), glm::vec3(0.0F, 0.0F, 0.0F), glm::vec3(0.0F, 0.0F, 1.0F)),
            .transform = glm::rotate(glm::scale(glm::mat4(1.0F), glm::vec3(10)), time * glm::radians(90.0F),
                                     glm::vec3(0.0F, 0.0F, 1.0F)),
        };
        void *ubo_data = nullptr;
        vmaMapMemory(allocator, uniform_buffer_allocations[image_index], &ubo_data);
        std::memcpy(ubo_data, &ubo, sizeof(UniformBuffer));
        vmaUnmapMemory(allocator, uniform_buffer_allocations[image_index]);

        std::array<VkPipelineStageFlags, 1> wait_stages{
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        };
        VkSubmitInfo submit_info{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &image_available,
            .pWaitDstStageMask = wait_stages.data(),
            .commandBufferCount = 1,
            .pCommandBuffers = &command_buffers[image_index],
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &rendering_finished,
        };
        vkQueueSubmit(graphics_queue, 1, &submit_info, fence);

        VkPresentInfoKHR present_info{
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &rendering_finished,
            .swapchainCount = 1,
            .pSwapchains = &swapchain,
            .pImageIndices = &image_index,
        };
        vkQueuePresentKHR(present_queue, &present_info);
        time += 0.01F;
    }

    vkDeviceWaitIdle(device);
    vkDestroySemaphore(device, rendering_finished, nullptr);
    vkDestroySemaphore(device, image_available, nullptr);
    vkDestroyFence(device, fence, nullptr);
    vkFreeCommandBuffers(device, command_pool, command_buffers.size(), command_buffers.data());
    vkDestroyCommandPool(device, command_pool, nullptr);
    vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
    for (std::size_t i = 0; i < swapchain_image_views.size(); i++) {
        vmaDestroyBuffer(allocator, uniform_buffers[i], uniform_buffer_allocations[i]);
    }
    vmaDestroyBuffer(allocator, index_buffer, index_buffer_allocation);
    vmaDestroyBuffer(allocator, vertex_buffer, vertex_buffer_allocation);
    vmaDestroyAllocator(allocator);
    for (auto *framebuffer : framebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyShaderModule(device, fragment_shader, nullptr);
    vkDestroyShaderModule(device, vertex_shader, nullptr);
    vkDestroyRenderPass(device, render_pass, nullptr);
    vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(device, ubo_layout, nullptr);
    for (auto *image_view : swapchain_image_views) {
        vkDestroyImageView(device, image_view, nullptr);
    }
    vkDestroySwapchainKHR(device, swapchain, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
}
