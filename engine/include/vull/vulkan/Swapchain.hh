#pragma once

#include <vull/maths/Vec.hh>
#include <vull/support/Span.hh>
#include <vull/support/Vector.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stdint.h>

namespace vull {

class VkContext;

class Swapchain {
    const VkContext &m_context;
    const vk::Extent2D m_extent;
    const vk::SurfaceKHR m_surface;
    vk::SurfaceCapabilitiesKHR m_surface_capabilities{};
    vk::SwapchainKHR m_swapchain{nullptr};
    Vector<vk::Image> m_images;
    Vector<vk::ImageView> m_image_views;
    vk::Queue m_present_queue{nullptr};

public:
    Swapchain(const VkContext &context, vk::Extent2D extent, vk::SurfaceKHR surface);
    Swapchain(const Swapchain &) = delete;
    Swapchain(Swapchain &&) = delete;
    ~Swapchain();

    Swapchain &operator=(const Swapchain &) = delete;
    Swapchain &operator=(Swapchain &&) = delete;

    uint32_t acquire_image(vk::Semaphore semaphore) const;
    void present(uint32_t image_index, Span<vk::Semaphore> wait_semaphores) const;

    Vec2f dimensions() const { return {static_cast<float>(m_extent.width), static_cast<float>(m_extent.height)}; }
    vk::Extent2D extent_2D() const { return m_extent; }
    vk::Extent3D extent_3D() const { return {m_extent.width, m_extent.height, 1}; }
    vk::Image image(uint32_t index) const { return m_images[index]; }
    vk::ImageView image_view(uint32_t index) const { return m_image_views[index]; }
};

} // namespace vull
