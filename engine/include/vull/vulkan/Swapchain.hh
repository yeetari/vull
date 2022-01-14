#pragma once

#include <vull/support/Span.hh>
#include <vull/support/Vector.hh>

#include <stdint.h>
#include <vulkan/vulkan_core.h>

namespace vull {

class Context;

class Swapchain {
    const Context &m_context;
    const VkExtent2D m_extent;
    const VkSurfaceKHR m_surface;
    VkSurfaceCapabilitiesKHR m_surface_capabilities{};
    VkSwapchainKHR m_swapchain{nullptr};
    Vector<VkImageView> m_image_views;
    VkQueue m_present_queue{nullptr};

public:
    Swapchain(const Context &context, VkExtent2D extent, VkSurfaceKHR surface);
    Swapchain(const Swapchain &) = delete;
    Swapchain(Swapchain &&) = delete;
    ~Swapchain();

    Swapchain &operator=(const Swapchain &) = delete;
    Swapchain &operator=(Swapchain &&) = delete;

    uint32_t acquire_image(VkSemaphore semaphore) const;
    void present(uint32_t image_index, Span<VkSemaphore> wait_semaphores) const;

    VkExtent2D extent() const { return m_extent; }
    const VkImageView &image_view(uint32_t index) const { return m_image_views[index]; }
};

} // namespace vull
