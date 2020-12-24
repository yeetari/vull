#pragma once

#include <vulkan/vulkan_core.h>

class Device;
class Instance;
class Window;

class Surface {
    const Instance &m_instance;
    VkSurfaceKHR m_surface{nullptr};
    VkSurfaceCapabilitiesKHR m_capabilities{};
    const VkExtent2D m_extent;

public:
    Surface(const Instance &instance, const Device &device, const Window &window);
    Surface(const Surface &) = delete;
    Surface(Surface &&) = delete;
    ~Surface();

    Surface &operator=(const Surface &) = delete;
    Surface &operator=(Surface &&) = delete;

    VkSurfaceKHR operator*() const { return m_surface; }
    const VkSurfaceCapabilitiesKHR &capabilities() const { return m_capabilities; }
    const VkExtent2D &extent() const { return m_extent; }
};
