#pragma once

#include <vull/container/vector.hh>
#include <vull/maths/vec.hh>
#include <vull/support/span.hh>
#include <vull/vulkan/vulkan.hh>

#include <stdint.h>

namespace vull::vk {

class Context;
class Image;

enum class SwapchainMode {
    LowPower,
    Normal,
    NoVsync,
};

class Swapchain {
    Context &m_context;
    vkb::Extent2D m_extent;
    vkb::SurfaceKHR m_surface;
    vkb::SurfaceCapabilitiesKHR m_surface_capabilities{};
    vkb::SwapchainKHR m_swapchain{nullptr};
    Vector<Image> m_images;

public:
    Swapchain(Context &context, vkb::Extent2D extent, vkb::SurfaceKHR surface, SwapchainMode mode);
    Swapchain(const Swapchain &) = delete;
    Swapchain(Swapchain &&);
    ~Swapchain();

    Swapchain &operator=(const Swapchain &) = delete;
    Swapchain &operator=(Swapchain &&) = delete;

    uint32_t acquire_image(vkb::Semaphore semaphore) const;
    void present(uint32_t image_index, Span<vkb::Semaphore> wait_semaphores) const;

    Context &context() const { return m_context; }
    Vec2u extent() const { return {m_extent.width, m_extent.height}; }
    Image &image(uint32_t index);
    const Image &image(uint32_t index) const;
    uint32_t image_count() const { return m_images.size(); }
};

} // namespace vull::vk
