#pragma once

#include <support/Span.hh>
#include <support/Vector.hh>

#include <vulkan/vulkan_core.h>

class Device;
class Surface;

class Swapchain {
    const Device &m_device;
    VkSwapchainKHR m_swapchain{nullptr};
    Vector<VkImageView> m_image_views;
    VkQueue m_present_queue{nullptr};

public:
    Swapchain(const Device &device, const Surface &surface);
    Swapchain(const Swapchain &) = delete;
    Swapchain(Swapchain &&) = delete;
    ~Swapchain();

    Swapchain &operator=(const Swapchain &) = delete;
    Swapchain &operator=(Swapchain &&) = delete;

    std::uint32_t acquire_next_image(VkSemaphore semaphore, VkFence fence);
    void present(std::uint32_t image_index, const Span<VkSemaphore> &wait_semaphores);

    VkSwapchainKHR operator*() const { return m_swapchain; }
    const Vector<VkImageView> &image_views() const { return m_image_views; }
};
