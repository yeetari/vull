#include <renderer/Swapchain.hh>

#include <renderer/Device.hh>
#include <renderer/Surface.hh>
#include <support/Assert.hh>
#include <support/Vector.hh>

#include <limits>

Swapchain::Swapchain(const Device &device, const Surface &surface) : m_device(device) {
    // Get present queue.
    for (std::uint32_t i = 0; const auto &queue_family : device.queue_families()) {
        VkBool32 present_supported = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device.physical(), i, *surface, &present_supported);
        if (present_supported == VK_TRUE) {
            vkGetDeviceQueue(*device, i, 0, &m_present_queue);
            break;
        }
    }
    ENSURE(m_present_queue != nullptr);

    // Create swapchain.
    VkSurfaceFormatKHR surface_format{
        .format = VK_FORMAT_B8G8R8A8_SRGB,
        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
    };
    VkSwapchainCreateInfoKHR swapchain_ci{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = *surface,
        .minImageCount = surface.capabilities().minImageCount + 1,
        .imageFormat = surface_format.format,
        .imageColorSpace = surface_format.colorSpace,
        .imageExtent = surface.extent(),
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = surface.capabilities().currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR,
        .clipped = VK_TRUE,
    };
    ENSURE(vkCreateSwapchainKHR(*device, &swapchain_ci, nullptr, &m_swapchain) == VK_SUCCESS);

    // Create image views.
    std::uint32_t image_count = 0;
    vkGetSwapchainImagesKHR(*device, m_swapchain, &image_count, nullptr);
    Vector<VkImage> images(image_count);
    vkGetSwapchainImagesKHR(*device, m_swapchain, &image_count, images.data());
    m_image_views.relength(image_count);
    for (std::uint32_t i = 0; i < image_count; i++) {
        VkImageViewCreateInfo image_view_ci{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = images[i],
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
        ENSURE(vkCreateImageView(*device, &image_view_ci, nullptr, &m_image_views[i]) == VK_SUCCESS);
    }
}

Swapchain::~Swapchain() {
    for (auto *image_view : m_image_views) {
        vkDestroyImageView(*m_device, image_view, nullptr);
    }
    vkDestroySwapchainKHR(*m_device, m_swapchain, nullptr);
}

std::uint32_t Swapchain::acquire_next_image(VkSemaphore semaphore, VkFence fence) {
    std::uint32_t image_index = 0;
    vkAcquireNextImageKHR(*m_device, m_swapchain, std::numeric_limits<std::uint64_t>::max(), semaphore, fence,
                          &image_index);
    return image_index;
}

void Swapchain::present(std::uint32_t image_index, const Span<VkSemaphore> &wait_semaphores) {
    VkPresentInfoKHR present_info{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = wait_semaphores.length(),
        .pWaitSemaphores = wait_semaphores.data(),
        .swapchainCount = 1,
        .pSwapchains = &m_swapchain,
        .pImageIndices = &image_index,
    };
    vkQueuePresentKHR(m_present_queue, &present_info);
}
