#include <vull/renderer/Swapchain.hh>

#include <vull/renderer/Device.hh>
#include <vull/renderer/Surface.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Log.hh>
#include <vull/support/Span.hh>
#include <vull/support/Vector.hh>

#include <algorithm>
#include <limits>

namespace {

const char *present_mode_str(VkPresentModeKHR mode) {
    switch (mode) {
    case VK_PRESENT_MODE_IMMEDIATE_KHR:
        return "VK_PRESENT_MODE_IMMEDIATE_KHR";
    case VK_PRESENT_MODE_MAILBOX_KHR:
        return "VK_PRESENT_MODE_MAILBOX_KHR";
    case VK_PRESENT_MODE_FIFO_KHR:
        return "VK_PRESENT_MODE_FIFO_KHR";
    case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
        return "VK_PRESENT_MODE_FIFO_RELAXED_KHR";
    default:
        return "unknown";
    }
}

const char *swapchain_mode_str(SwapchainMode mode) {
    switch (mode) {
    case SwapchainMode::LowLatency:
        return "SwapchainMode::LowLatency";
    case SwapchainMode::LowPower:
        return "SwapchainMode::LowPower";
    case SwapchainMode::Normal:
        return "SwapchainMode::Normal";
    case SwapchainMode::NoVsync:
        return "SwapchainMode::NoVsync";
    default:
        ENSURE_NOT_REACHED();
    }
}

int rate_present_mode(VkPresentModeKHR present_mode, SwapchainMode swapchain_mode) {
    switch (present_mode) {
    case VK_PRESENT_MODE_IMMEDIATE_KHR:
        return swapchain_mode == SwapchainMode::NoVsync ? 100000 : 100;
    case VK_PRESENT_MODE_MAILBOX_KHR:
        return swapchain_mode == SwapchainMode::LowLatency ? 100000 : 300;
    case VK_PRESENT_MODE_FIFO_KHR:
        return swapchain_mode == SwapchainMode::LowPower ? 100000 : 200;
    case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
        return 400;
    default:
        return -100;
    }
}

} // namespace

Swapchain::Swapchain(const Device &device, const Surface &surface, SwapchainMode mode)
    : m_device(device), m_extent{surface.extent().width, surface.extent().height, 1},
      m_format(VK_FORMAT_B8G8R8A8_SRGB) {
    // Get present queue.
    for (std::uint32_t i = 0; i < device.queue_families().size(); i++) {
        VkBool32 present_supported = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device.physical(), i, *surface, &present_supported);
        if (present_supported == VK_TRUE) {
            Log::trace("renderer", "Using queue %d:0 for presenting", i);
            vkGetDeviceQueue(*device, i, 0, &m_present_queue);
            break;
        }
    }
    ENSURE(m_present_queue != nullptr);

    // Pick best present mode.
    std::uint32_t present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device.physical(), *surface, &present_mode_count, nullptr);
    Vector<VkPresentModeKHR> present_modes(present_mode_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device.physical(), *surface, &present_mode_count, present_modes.data());
    Log::trace("renderer", "Surface supports %d present modes", present_modes.size());
    for (auto present_mode : present_modes) {
        Log::trace("renderer", " - %s", present_mode_str(present_mode));
    }
    std::sort(present_modes.begin(), present_modes.end(), [mode](VkPresentModeKHR lhs, VkPresentModeKHR rhs) {
        return rate_present_mode(lhs, mode) > rate_present_mode(rhs, mode);
    });
    Log::info("renderer", "Requested %s", swapchain_mode_str(mode));
    Log::debug("renderer", " - Using %s", present_mode_str(*present_modes.begin()));

    // Create swapchain.
    Log::debug("renderer", "Creating swapchain");
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
        .presentMode = *present_modes.begin(),
        .clipped = VK_TRUE,
    };
    ENSURE(vkCreateSwapchainKHR(*device, &swapchain_ci, nullptr, &m_swapchain) == VK_SUCCESS);

    // Create image views.
    std::uint32_t image_count = 0;
    vkGetSwapchainImagesKHR(*device, m_swapchain, &image_count, nullptr);
    Vector<VkImage> images(image_count);
    vkGetSwapchainImagesKHR(*device, m_swapchain, &image_count, images.data());
    m_image_views.resize(image_count);
    Log::trace("renderer", " - Creating %d image views", image_count);
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

std::uint32_t Swapchain::acquire_next_image(VkSemaphore semaphore, VkFence fence) const {
    std::uint32_t image_index = 0;
    vkAcquireNextImageKHR(*m_device, m_swapchain, std::numeric_limits<std::uint64_t>::max(), semaphore, fence,
                          &image_index);
    return image_index;
}

void Swapchain::present(std::uint32_t image_index, Span<VkSemaphore> wait_semaphores) const {
    VkPresentInfoKHR present_info{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = static_cast<std::uint32_t>(wait_semaphores.size()),
        .pWaitSemaphores = wait_semaphores.data(),
        .swapchainCount = 1,
        .pSwapchains = &m_swapchain,
        .pImageIndices = &image_index,
    };
    vkQueuePresentKHR(m_present_queue, &present_info);
}
