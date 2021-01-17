#pragma once

#include <vull/support/Span.hh>
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
    void present(std::uint32_t image_index, const Span<VkSemaphore> &wait_semaphores) const;

    VkSwapchainKHR operator*() const { return m_swapchain; }
    const Vector<VkImageView> &image_views() const { return m_image_views; }
};
