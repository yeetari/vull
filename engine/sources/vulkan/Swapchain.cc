#include <vull/vulkan/Swapchain.hh>

#include <vull/maths/Common.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Lsan.hh>
#include <vull/support/Span.hh>
#include <vull/support/Vector.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/Vulkan.hh>

namespace vull {

Swapchain::Swapchain(const Context &context, vk::Extent2D extent, vk::SurfaceKHR surface)
    : m_context(context), m_extent(extent), m_surface(surface) {
    vk::SurfaceFormatKHR surface_format{
        .format = vk::Format::B8G8R8A8Srgb,
        .colorSpace = vk::ColorSpaceKHR::SrgbNonlinearKHR,
    };
    VULL_ENSURE(context.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_surface, &m_surface_capabilities) ==
                vk::Result::Success);

    vk::SwapchainCreateInfoKHR swapchain_ci{
        .sType = vk::StructureType::SwapchainCreateInfoKHR,
        .surface = surface,
        .minImageCount = min(m_surface_capabilities.minImageCount + 1,
                             m_surface_capabilities.maxImageCount != 0u ? m_surface_capabilities.maxImageCount : ~0u),
        .imageFormat = surface_format.format,
        .imageColorSpace = surface_format.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsage::ColorAttachment,
        .imageSharingMode = vk::SharingMode::Exclusive,
        .preTransform = m_surface_capabilities.currentTransform,
        .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::OpaqueKHR,
        .presentMode = vk::PresentModeKHR::FifoKHR,
        .clipped = vk::VK_TRUE,
    };
    VULL_ENSURE(context.vkCreateSwapchainKHR(&swapchain_ci, &m_swapchain) == vk::Result::Success);

    uint32_t image_count = 0;
    context.vkGetSwapchainImagesKHR(m_swapchain, &image_count, nullptr);
    m_images.ensure_size(image_count);
    m_image_views.ensure_size(image_count);
    context.vkGetSwapchainImagesKHR(m_swapchain, &image_count, m_images.data());
    for (uint32_t i = 0; i < image_count; i++) {
        vk::ImageViewCreateInfo image_view_ci{
            .sType = vk::StructureType::ImageViewCreateInfo,
            .image = m_images[i],
            .viewType = vk::ImageViewType::_2D,
            .format = surface_format.format,
            .components{
                vk::ComponentSwizzle::Identity,
                vk::ComponentSwizzle::Identity,
                vk::ComponentSwizzle::Identity,
                vk::ComponentSwizzle::Identity,
            },
            .subresourceRange{
                .aspectMask = vk::ImageAspect::Color,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        VULL_ENSURE(context.vkCreateImageView(&image_view_ci, &m_image_views[i]) == vk::Result::Success);
    }

    // Find a present queue.
    for (uint32_t i = 0; i < context.queue_families().size(); i++) {
        vk::Bool32 present_supported = vk::VK_FALSE;
        context.vkGetPhysicalDeviceSurfaceSupportKHR(i, m_surface, &present_supported);
        if (present_supported == vk::VK_TRUE) {
            context.vkGetDeviceQueue(i, 0, &m_present_queue);
            return;
        }
    }
    VULL_ENSURE_NOT_REACHED("Failed to find a present queue");
}

Swapchain::~Swapchain() {
    for (auto *image_view : m_image_views) {
        m_context.vkDestroyImageView(image_view);
    }
    m_context.vkDestroySwapchainKHR(m_swapchain);
    m_context.vkDestroySurfaceKHR(m_surface);
}

uint32_t Swapchain::acquire_image(vk::Semaphore semaphore) const {
    LsanDisabler lsan_disabler;
    uint32_t image_index = 0;
    m_context.vkAcquireNextImageKHR(m_swapchain, ~0ull, semaphore, nullptr, &image_index);
    return image_index;
}

void Swapchain::present(uint32_t image_index, Span<vk::Semaphore> wait_semaphores) const {
    vk::PresentInfoKHR present_info{
        .sType = vk::StructureType::PresentInfoKHR,
        .waitSemaphoreCount = wait_semaphores.size(),
        .pWaitSemaphores = wait_semaphores.data(),
        .swapchainCount = 1,
        .pSwapchains = &m_swapchain,
        .pImageIndices = &image_index,
    };
    m_context.vkQueuePresentKHR(m_present_queue, &present_info);
}

} // namespace vull
