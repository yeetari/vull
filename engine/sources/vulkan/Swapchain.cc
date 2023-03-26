#include <vull/vulkan/Swapchain.hh>

#include <vull/core/Log.hh>
#include <vull/maths/Common.hh>
#include <vull/support/Algorithm.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Enum.hh>
#include <vull/support/Span.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/Image.hh>
#include <vull/vulkan/Queue.hh>
#include <vull/vulkan/Vulkan.hh>

namespace vull::vk {
namespace {

// LowPower     Fifo
// Normal       Mailbox -> FifoRelaxed -> Fifo
// NoVsync      Immediate -> Mailbox -> FifoRelaxed -> Fifo
unsigned rate_present_mode(vkb::PresentModeKHR present_mode, SwapchainMode swapchain_mode) {
    if (present_mode == vkb::PresentModeKHR::Fifo) {
        return 1;
    }
    switch (swapchain_mode) {
    case SwapchainMode::Normal:
        switch (present_mode) {
        case vkb::PresentModeKHR::Mailbox:
            return 3;
        case vkb::PresentModeKHR::FifoRelaxed:
            return 2;
        default:
            return 0;
        }
    case SwapchainMode::NoVsync:
        switch (present_mode) {
        case vkb::PresentModeKHR::Immediate:
            return 4;
        case vkb::PresentModeKHR::Mailbox:
            return 3;
        case vkb::PresentModeKHR::FifoRelaxed:
            return 2;
        default:
            return 0;
        }
    default:
        return 0;
    }
}

} // namespace

Swapchain::Swapchain(Context &context, vkb::Extent2D extent, vkb::SurfaceKHR surface, SwapchainMode mode)
    : m_context(context), m_extent(extent), m_surface(surface) {
    vkb::SurfaceFormatKHR surface_format{
        .format = vkb::Format::B8G8R8A8Unorm,
        .colorSpace = vkb::ColorSpaceKHR::SrgbNonlinear,
    };
    VULL_ENSURE(context.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_surface, &m_surface_capabilities) ==
                vkb::Result::Success);

    // Pick best present mode.
    uint32_t present_mode_count = 0;
    context.vkGetPhysicalDeviceSurfacePresentModesKHR(surface, &present_mode_count, nullptr);
    Vector<vkb::PresentModeKHR> present_modes(present_mode_count);
    context.vkGetPhysicalDeviceSurfacePresentModesKHR(surface, &present_mode_count, present_modes.data());
    vull::sort(present_modes, [mode](vkb::PresentModeKHR lhs, vkb::PresentModeKHR rhs) {
        return rate_present_mode(lhs, mode) < rate_present_mode(rhs, mode);
    });

    vull::info("[vulkan] Requested {}", vull::enum_name(mode));
    vull::debug("[vulkan]  - using {}", vull::enum_name<3>(present_modes.first()));

    vkb::SwapchainCreateInfoKHR swapchain_ci{
        .sType = vkb::StructureType::SwapchainCreateInfoKHR,
        .surface = surface,
        .minImageCount = min(m_surface_capabilities.minImageCount + 1,
                             m_surface_capabilities.maxImageCount != 0u ? m_surface_capabilities.maxImageCount : ~0u),
        .imageFormat = surface_format.format,
        .imageColorSpace = surface_format.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = vkb::ImageUsage::ColorAttachment | vkb::ImageUsage::Storage,
        .imageSharingMode = vkb::SharingMode::Exclusive,
        .preTransform = m_surface_capabilities.currentTransform,
        .compositeAlpha = vkb::CompositeAlphaFlagsKHR::Opaque,
        .presentMode = present_modes.first(),
        .clipped = true,
    };
    VULL_ENSURE(context.vkCreateSwapchainKHR(&swapchain_ci, &m_swapchain) == vkb::Result::Success);

    uint32_t image_count = 0;
    context.vkGetSwapchainImagesKHR(m_swapchain, &image_count, nullptr);
    Vector<vkb::Image> images(image_count);
    context.vkGetSwapchainImagesKHR(m_swapchain, &image_count, images.data());
    for (vkb::Image image : images) {
        vkb::ImageViewCreateInfo view_ci{
            .sType = vkb::StructureType::ImageViewCreateInfo,
            .image = image,
            .viewType = vkb::ImageViewType::_2D,
            .format = surface_format.format,
            .subresourceRange{
                .aspectMask = vkb::ImageAspect::Color,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        vkb::ImageView view;
        VULL_ENSURE(context.vkCreateImageView(&view_ci, &view) == vkb::Result::Success);
        m_images.push(Image(context, view_ci.format, ImageView(&context, image, view, view_ci.subresourceRange)));
    }

    // Ensure graphics queue supports presenting.
    vkb::Bool present_supported = false;
    context.vkGetPhysicalDeviceSurfaceSupportKHR(m_context.graphics_queue().family_index(), m_surface,
                                                 &present_supported);
    VULL_ENSURE(present_supported);
}

Swapchain::~Swapchain() {
    m_context.vkDestroySwapchainKHR(m_swapchain);
    m_context.vkDestroySurfaceKHR(m_surface);
}

uint32_t Swapchain::acquire_image(vkb::Semaphore semaphore) const {
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
    m_context.vkQueuePresentKHR(*m_context.graphics_queue(), &present_info);
}

Image &Swapchain::image(uint32_t index) {
    return m_images[index];
}

const Image &Swapchain::image(uint32_t index) const {
    return m_images[index];
}

} // namespace vull::vk
