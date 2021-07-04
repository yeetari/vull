#include <vull/rendering/RenderTexture.hh>

#include <vull/vulkan/Device.hh>

RenderTexture::RenderTexture(TextureType type, MemoryUsage usage) : MemoryResource(usage), m_type(type) {
    switch (type) {
    case TextureType::Depth:
        set_name("depth buffer");
        break;
    case TextureType::Swapchain:
        set_name("back buffer");
        break;
    default:
        break;
    }
}

RenderTexture::~RenderTexture() {
    if (m_type != TextureType::Swapchain) {
        vkDestroyImageView(**m_device, m_image_view, nullptr);
    }
    vkDestroyImage(**m_device, m_image, nullptr);
    vkFreeMemory(**m_device, m_memory, nullptr);
}

void RenderTexture::set_clear_colour(float r, float g, float b, float a) {
    m_clear_value.color = {{r, g, b, a}};
}

void RenderTexture::set_clear_depth_stencil(float depth, std::uint32_t stencil) {
    m_clear_value.depthStencil = {
        .depth = depth,
        .stencil = stencil,
    };
}

void RenderTexture::set_extent(VkExtent3D extent) {
    m_extent = extent;
}

void RenderTexture::set_format(VkFormat format) {
    m_format = format;
}

void RenderTexture::build_objects(const Device &device, ExecutableGraph *executable_graph) {
    RenderResource::build_objects(device, executable_graph);
    if (m_type == TextureType::Swapchain) {
        return;
    }

    ASSERT(m_usage == MemoryUsage::GpuOnly);
    ASSERT(m_extent.width != 0 && m_extent.height != 0 && m_extent.depth != 0);

    VkImageUsageFlags usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    VkImageCreateInfo image_ci{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = m_format,
        .extent = m_extent,
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    ENSURE(vkCreateImage(*device, &image_ci, nullptr, &m_image) == VK_SUCCESS);

    VkMemoryRequirements memory_requirements;
    vkGetImageMemoryRequirements(*device, m_image, &memory_requirements);
    m_memory = device.allocate_memory(memory_requirements, MemoryType::GpuOnly, true, nullptr, m_image);
    ENSURE(vkBindImageMemory(*device, m_image, m_memory, 0) == VK_SUCCESS);

    VkImageViewCreateInfo image_view_ci{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = m_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = m_format,
        .subresourceRange{
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .levelCount = 1,
            .layerCount = 1,
        },
    };
    ENSURE(vkCreateImageView(*device, &image_view_ci, nullptr, &m_image_view) == VK_SUCCESS);
}

void RenderTexture::transfer(const void *, VkDeviceSize) {
    ASSERT(m_executable_graph != nullptr);
    ASSERT(m_usage == MemoryUsage::Transfer);
    ENSURE_NOT_REACHED();
}
