#pragma once

#include <vull/maths/Vec.hh>
#include <vull/support/Span.hh>
#include <vull/support/Vector.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stdint.h>

namespace vull::vk {

class Context;

enum class SwapchainMode {
    LowPower,
    Normal,
    NoVsync,
};

class Swapchain {
    const Context &m_context;
    const vkb::Extent2D m_extent;
    const vkb::SurfaceKHR m_surface;
    vkb::SurfaceCapabilitiesKHR m_surface_capabilities{};
    vkb::SwapchainKHR m_swapchain{nullptr};
    Vector<vkb::Image> m_images;
    Vector<vkb::ImageView> m_image_views;
    vkb::Queue m_present_queue{nullptr};

public:
    Swapchain(const Context &context, vkb::Extent2D extent, vkb::SurfaceKHR surface, SwapchainMode mode);
    Swapchain(const Swapchain &) = delete;
    Swapchain(Swapchain &&) = delete;
    ~Swapchain();

    Swapchain &operator=(const Swapchain &) = delete;
    Swapchain &operator=(Swapchain &&) = delete;

    uint32_t acquire_image(vkb::Semaphore semaphore) const;
    void present(uint32_t image_index, Span<vkb::Semaphore> wait_semaphores) const;

    const Context &context() const { return m_context; }
    Vec2f dimensions() const { return {static_cast<float>(m_extent.width), static_cast<float>(m_extent.height)}; }
    vkb::Extent2D extent_2D() const { return m_extent; }
    vkb::Extent3D extent_3D() const { return {m_extent.width, m_extent.height, 1}; }
    vkb::Image image(uint32_t index) const { return m_images[index]; }
    vkb::ImageView image_view(uint32_t index) const { return m_image_views[index]; }
    vkb::Queue present_queue() const { return m_present_queue; }
};

} // namespace vull::vk
