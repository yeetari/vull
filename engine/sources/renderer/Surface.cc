#include <vull/renderer/Surface.hh>

#include <vull/io/Window.hh>
#include <vull/renderer/Device.hh>
#include <vull/renderer/Instance.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Log.hh>

#include <GLFW/glfw3.h>

Surface::Surface(const Instance &instance, const Device &device, const Window &window)
    : m_instance(instance), m_extent{window.width(), window.height()} {
    Log::trace("renderer", "Creating surface");
    ENSURE(glfwCreateWindowSurface(*instance, *window, nullptr, &m_surface) == VK_SUCCESS);
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.physical(), m_surface, &m_capabilities);
    Log::trace("renderer", " - minImageCount=%d", m_capabilities.minImageCount);
    Log::trace("renderer", " - maxImageCount=%d", m_capabilities.maxImageCount);
}

Surface::~Surface() {
    vkDestroySurfaceKHR(*m_instance, m_surface, nullptr);
}
