#pragma once

#include <vull/support/Span.hh> // IWYU pragma: keep
#include <vull/support/Vector.hh>

#include <vulkan/vulkan_core.h>

#include <cstdint>

class Device;
class Surface;

enum class SwapchainMode {
    LowLatency,
    LowPower,
    Normal,
    NoVsync,
};

class Swapchain {
    const Device &m_device;
    const VkExtent3D m_extent;
    const VkFormat m_format;
    VkSwapchainKHR m_swapchain{nullptr};
    Vector<VkImageView> m_image_views;
    VkQueue m_present_queue{nullptr};

public:
    Swapchain(const Device &device, const Surface &surface, SwapchainMode mode);
    Swapchain(const Swapchain &) = delete;
    Swapchain(Swapchain &&) = delete;
    ~Swapchain();

    Swapchain &operator=(const Swapchain &) = delete;
    Swapchain &operator=(Swapchain &&) = delete;

    std::uint32_t acquire_next_image(VkSemaphore semaphore, VkFence fence) const;
    void present(std::uint32_t image_index, Span<VkSemaphore> wait_semaphores) const;

    VkExtent3D extent() const { return m_extent; }
    VkFormat format() const { return m_format; }
    VkSwapchainKHR operator*() const { return m_swapchain; }
    std::uint32_t image_count() const { return m_image_views.size(); }
    const Vector<VkImageView> &image_views() const { return m_image_views; }
};
