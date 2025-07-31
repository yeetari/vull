#include <vull/vulkan/swapchain.hh>

#include <vull/container/vector.hh>
#include <vull/core/log.hh>
#include <vull/maths/common.hh>
#include <vull/maths/relational.hh>
#include <vull/maths/vec.hh>
#include <vull/support/algorithm.hh>
#include <vull/support/assert.hh>
#include <vull/support/enum.hh>
#include <vull/support/optional.hh>
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

Swapchain::Swapchain(Context &context, vkb::SurfaceKHR surface, SwapchainMode mode)
    : m_context(context), m_surface(surface) {
    // Pick best present mode.
    uint32_t present_mode_count = 0;
    context.vkGetPhysicalDeviceSurfacePresentModesKHR(surface, &present_mode_count, nullptr);
    Vector<vkb::PresentModeKHR> present_modes(present_mode_count);
    context.vkGetPhysicalDeviceSurfacePresentModesKHR(surface, &present_mode_count, present_modes.data());
    vull::sort(present_modes, [mode](vkb::PresentModeKHR lhs, vkb::PresentModeKHR rhs) {
        return rate_present_mode(lhs, mode) < rate_present_mode(rhs, mode);
    });

    m_present_mode = present_modes.first();
    vull::info("[vulkan] Requested {}", vull::enum_name(mode));
    vull::debug("[vulkan]  - using {}", vull::enum_name<3>(m_present_mode));
}

Swapchain::~Swapchain() {
    m_images.clear();
    m_context.vkDestroySwapchainKHR(m_old_swapchain);
    m_context.vkDestroySwapchainKHR(m_swapchain);
    m_context.vkDestroySurfaceKHR(m_surface);
}

Swapchain::Swapchain(Swapchain &&other) : m_context(other.m_context) {
    m_surface = vull::exchange(other.m_surface, nullptr);
    m_present_mode = vull::exchange(other.m_present_mode, {});
    m_extent = vull::exchange(other.m_extent, {});
    m_swapchain = vull::exchange(other.m_swapchain, nullptr);
    m_old_swapchain = vull::exchange(other.m_old_swapchain, nullptr);
    m_images = vull::move(other.m_images);
    m_recreate_required = vull::exchange(other.m_recreate_required, true);
}

Optional<uint32_t> Swapchain::acquire_image(vkb::Semaphore semaphore) {
    uint32_t image_index = 0;
    const auto result = m_context.vkAcquireNextImageKHR(m_swapchain, ~0uz, semaphore, nullptr, &image_index);
    switch (result) {
    case vkb::Result::SuboptimalKHR:
        m_recreate_required = true;
        [[fallthrough]];
    case vkb::Result::Success:
        return image_index;
    case vkb::Result::ErrorOutOfDateKHR:
        m_recreate_required = true;
        return vull::nullopt;
    default:
        // TODO: Print result string.
        VULL_ENSURE_NOT_REACHED("Swapchain acquire failed");
    }
}

void Swapchain::present(uint32_t image_index, Span<vkb::Semaphore> wait_semaphores) {
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
    const auto present_result = queue.present(present_info);
    if (present_result == vkb::Result::SuboptimalKHR || present_result == vkb::Result::ErrorOutOfDateKHR) {
        m_recreate_required = true;
    } else if (present_result != vkb::Result::Success) {
        // TODO: Print result string.
        VULL_ENSURE_NOT_REACHED("Swapchain present failed");
    }
}

void Swapchain::recreate(Vec2u extent) {
    vkb::SurfaceCapabilitiesKHR surface_capabilities{};
    VULL_ENSURE(m_context.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_surface, &surface_capabilities) ==
                vkb::Result::Success);

    uint32_t min_image_count = vull::max(surface_capabilities.minImageCount + 1, 3);
    if (surface_capabilities.maxImageCount != 0) {
        min_image_count = vull::min(min_image_count, surface_capabilities.maxImageCount);
    }

    // Clamp window extent between allowed values.
    const auto min_extent = surface_capabilities.minImageExtent;
    const auto max_extent = surface_capabilities.maxImageExtent;
    m_extent.width = vull::clamp(extent.x(), min_extent.width, max_extent.width);
    m_extent.height = vull::clamp(extent.y(), min_extent.height, max_extent.height);

    // Destroy the old swapchain and swap it to our current one.
    m_images.clear();
    m_context.vkDestroySwapchainKHR(vull::exchange(m_old_swapchain, m_swapchain));

    // TODO: Don't hardcode surface format.
    vkb::SurfaceFormatKHR surface_format{
        .format = vkb::Format::B8G8R8A8Srgb,
        .colorSpace = vkb::ColorSpaceKHR::SrgbNonlinear,
    };
    vkb::SwapchainCreateInfoKHR swapchain_ci{
        .sType = vkb::StructureType::SwapchainCreateInfoKHR,
        .surface = m_surface,
        .minImageCount = min_image_count,
        .imageFormat = surface_format.format,
        .imageColorSpace = surface_format.colorSpace,
        .imageExtent = m_extent,
        .imageArrayLayers = 1,
        .imageUsage = vkb::ImageUsage::ColorAttachment,
        .imageSharingMode = vkb::SharingMode::Exclusive,
        .preTransform = surface_capabilities.currentTransform,
        .compositeAlpha = vkb::CompositeAlphaFlagsKHR::Opaque,
        .presentMode = m_present_mode,
        .clipped = true,
        .oldSwapchain = m_old_swapchain,
    };
    VULL_ENSURE(m_context.vkCreateSwapchainKHR(&swapchain_ci, &m_swapchain) == vkb::Result::Success);

    uint32_t image_count = 0;
    m_context.vkGetSwapchainImagesKHR(m_swapchain, &image_count, nullptr);
    Vector<vkb::Image> images(image_count);
    m_context.vkGetSwapchainImagesKHR(m_swapchain, &image_count, images.data());
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
        vkb::Extent3D extent_3D{m_extent.width, m_extent.height, 1};
        vkb::ImageView view;
        VULL_ENSURE(m_context.vkCreateImageView(&view_ci, &view) == vkb::Result::Success);
        m_images.push(
            Image(m_context, extent_3D, view_ci.format, ImageView(&m_context, image, view, view_ci.subresourceRange)));
    }
    m_recreate_required = false;
}

bool Swapchain::is_recreate_required(Vec2u window_extent) const {
    // Proactively recreate on window-swapchain extent mismatch.
    if (vull::any(vull::not_equal(window_extent, extent()))) {
        return true;
    }
    return m_recreate_required;
}

Image &Swapchain::image(uint32_t index) {
    return m_images[index];
}

const Image &Swapchain::image(uint32_t index) const {
    return m_images[index];
}

} // namespace vull::vk
