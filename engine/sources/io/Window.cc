#include <vull/io/Window.hh>

#include <vull/support/Assert.hh>
#include <vull/support/Log.hh>

#include <GLFW/glfw3.h>

void Window::poll_events() {
    glfwPollEvents();
}

Window::Window() {
    Log::trace("io", "Initialising GLFW");
    ENSURE(glfwInit() == GLFW_TRUE);
    const auto *video_mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    m_width = video_mode->width;
    m_height = video_mode->height;
    Log::info("io", "Creating window with dimensions %dx%d", m_width, m_height);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    m_window = glfwCreateWindow(m_width, m_height, "vull", glfwGetPrimaryMonitor(), nullptr);
    ENSURE(m_window != nullptr);
}

Window::~Window() {
    Log::debug("io", "Destroying window and terminating GLFW");
    glfwDestroyWindow(m_window);
    glfwTerminate();
}

float Window::aspect_ratio() const {
    return static_cast<float>(m_width) / static_cast<float>(m_height);
}

bool Window::should_close() const {
    return glfwWindowShouldClose(m_window) == GLFW_TRUE;
}

void Window::close() const {
    Log::trace("io", "Window close requested");
    glfwSetWindowShouldClose(m_window, GLFW_TRUE);
}
