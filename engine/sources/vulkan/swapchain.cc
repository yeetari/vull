#include <vull/vulkan/swapchain.hh>

#include <vull/container/vector.hh>
#include <vull/core/log.hh>
#include <vull/maths/common.hh>
#include <vull/support/algorithm.hh>
#include <vull/support/assert.hh>
#include <vull/support/enum.hh>
#include <vull/support/span.hh>
#include <vull/support/utility.hh>
#include <vull/vulkan/context.hh>
#include <vull/vulkan/image.hh>
#include <vull/vulkan/queue.hh>
#include <vull/vulkan/vulkan.hh>

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
        .format = vkb::Format::B8G8R8A8Srgb,
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
        .imageUsage = vkb::ImageUsage::ColorAttachment,
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
        vkb::Extent3D extent{m_extent.width, m_extent.height, 1};
        vkb::ImageView view;
        VULL_ENSURE(context.vkCreateImageView(&view_ci, &view) == vkb::Result::Success);
        m_images.push(
            Image(context, extent, view_ci.format, ImageView(&context, image, view, view_ci.subresourceRange)));
    }
}

Swapchain::~Swapchain() {
    m_context.vkDestroySwapchainKHR(m_swapchain);
    m_context.vkDestroySurfaceKHR(m_surface);
}

Swapchain::Swapchain(Swapchain &&other)
    : m_context(other.m_context), m_extent(vull::exchange(other.m_extent, {})),
      m_surface(vull::exchange(other.m_surface, nullptr)),
      m_surface_capabilities(vull::exchange(other.m_surface_capabilities, {})),
      m_swapchain(vull::exchange(other.m_swapchain, nullptr)), m_images(vull::move(other.m_images)) {}

uint32_t Swapchain::acquire_image(vkb::Semaphore semaphore) const {
    uint32_t image_index = 0;
    m_context.vkAcquireNextImageKHR(m_swapchain, ~0uz, semaphore, nullptr, &image_index);
    return image_index;
}

void Swapchain::present(uint32_t image_index, Span<vkb::Semaphore> wait_semaphores) const {
    vkb::PresentInfoKHR present_info{
        .sType = vkb::StructureType::PresentInfoKHR,
        .waitSemaphoreCount = static_cast<uint32_t>(wait_semaphores.size()),
        .pWaitSemaphores = wait_semaphores.data(),
        .swapchainCount = 1,
        .pSwapchains = &m_swapchain,
        .pImageIndices = &image_index,
    };
    // TODO: Assuming graphics queue last touched swapchain image + that whatever graphics queue we get can present.
    auto &queue = m_context.get_queue(vk::QueueKind::Graphics);
    queue.present(present_info);
}

Image &Swapchain::image(uint32_t index) {
    return m_images[index];
}

const Image &Swapchain::image(uint32_t index) const {
    return m_images[index];
}

} // namespace vull::vk
