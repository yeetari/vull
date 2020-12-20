#include <Window.hh>

#include <support/Assert.hh>

#include <GLFW/glfw3.h>

void Window::poll_events() {
    glfwPollEvents();
}

Window::Window(int width, int height) : m_width(width), m_height(height) {
    ENSURE(glfwInit() == GLFW_TRUE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    m_window = glfwCreateWindow(width, height, "vull", glfwGetPrimaryMonitor(), nullptr);
    ENSURE(m_window != nullptr);
}

Window::~Window() {
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
    glfwSetWindowShouldClose(m_window, GLFW_TRUE);
}
