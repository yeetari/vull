#include <vull/vulkan/Swapchain.hh>

#include <vull/maths/Common.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Lsan.hh>
#include <vull/support/Span.hh>
#include <vull/support/Vector.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/Vulkan.hh>

namespace vull {

Swapchain::Swapchain(const VkContext &context, vkb::Extent2D extent, vkb::SurfaceKHR surface)
    : m_context(context), m_extent(extent), m_surface(surface) {
    vkb::SurfaceFormatKHR surface_format{
        .format = vkb::Format::B8G8R8A8Unorm,
        .colorSpace = vkb::ColorSpaceKHR::SrgbNonlinearKHR,
    };
    VULL_ENSURE(context.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_surface, &m_surface_capabilities) ==
                vkb::Result::Success);

    vkb::SwapchainCreateInfoKHR swapchain_ci{
        .sType = vkb::StructureType::SwapchainCreateInfoKHR,
        .surface = surface,
        .minImageCount = min(m_surface_capabilities.minImageCount + 1,
                             m_surface_capabilities.maxImageCount != 0u ? m_surface_capabilities.maxImageCount : ~0u),
        .imageFormat = surface_format.format,
        .imageColorSpace = surface_format.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = vkb::ImageUsage::ColorAttachment | vkb::ImageUsage::Storage, // TODO: Still need ColorAttachment?
        .imageSharingMode = vkb::SharingMode::Exclusive,
        .preTransform = m_surface_capabilities.currentTransform,
        .compositeAlpha = vkb::CompositeAlphaFlagBitsKHR::OpaqueKHR,
        .presentMode = vkb::PresentModeKHR::FifoKHR,
        .clipped = true,
    };
    VULL_ENSURE(context.vkCreateSwapchainKHR(&swapchain_ci, &m_swapchain) == vkb::Result::Success);

    uint32_t image_count = 0;
    context.vkGetSwapchainImagesKHR(m_swapchain, &image_count, nullptr);
    m_images.ensure_size(image_count);
    m_image_views.ensure_size(image_count);
    context.vkGetSwapchainImagesKHR(m_swapchain, &image_count, m_images.data());
    for (uint32_t i = 0; i < image_count; i++) {
        vkb::ImageViewCreateInfo image_view_ci{
            .sType = vkb::StructureType::ImageViewCreateInfo,
            .image = m_images[i],
            .viewType = vkb::ImageViewType::_2D,
            .format = surface_format.format,
            .subresourceRange{
                .aspectMask = vkb::ImageAspect::Color,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        VULL_ENSURE(context.vkCreateImageView(&image_view_ci, &m_image_views[i]) == vkb::Result::Success);
    }

    // Find a present queue.
    for (uint32_t i = 0; i < context.queue_families().size(); i++) {
        vkb::Bool present_supported = false;
        context.vkGetPhysicalDeviceSurfaceSupportKHR(i, m_surface, &present_supported);
        if (present_supported) {
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

uint32_t Swapchain::acquire_image(vkb::Semaphore semaphore) const {
    LsanDisabler lsan_disabler;
    uint32_t image_index = 0;
    m_context.vkAcquireNextImageKHR(m_swapchain, ~0ull, semaphore, nullptr, &image_index);
    return image_index;
}

void Swapchain::present(uint32_t image_index, Span<vkb::Semaphore> wait_semaphores) const {
    vkb::PresentInfoKHR present_info{
        .sType = vkb::StructureType::PresentInfoKHR,
        .waitSemaphoreCount = wait_semaphores.size(),
        .pWaitSemaphores = wait_semaphores.data(),
        .swapchainCount = 1,
        .pSwapchains = &m_swapchain,
        .pImageIndices = &image_index,
    };
    m_context.vkQueuePresentKHR(m_present_queue, &present_info);
}

} // namespace vull
