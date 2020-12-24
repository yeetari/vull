#include <renderer/Surface.hh>

#include <Window.hh>
#include <renderer/Device.hh>
#include <renderer/Instance.hh>
#include <support/Assert.hh>

#include <GLFW/glfw3.h>

Surface::Surface(const Instance &instance, const Device &device, const Window &window)
    : m_instance(instance), m_extent{window.width(), window.height()} {
    ENSURE(glfwCreateWindowSurface(*instance, *window, nullptr, &m_surface) == VK_SUCCESS);
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.physical(), m_surface, &m_capabilities);
}

Surface::~Surface() {
    vkDestroySurfaceKHR(*m_instance, m_surface, nullptr);
}
