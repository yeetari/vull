#pragma once

#include <cstdint>

struct GLFWwindow;

class Window {
    const std::uint32_t m_width;
    const std::uint32_t m_height;
    GLFWwindow *m_window;

public:
    static void poll_events();

    Window(std::uint32_t width, std::uint32_t height);
    Window(const Window &) = delete;
    Window(Window &&) = delete;
    ~Window();

    Window &operator=(const Window &) = delete;
    Window &operator=(Window &&) = delete;

    float aspect_ratio() const;
    bool should_close() const;
    void close() const;

    GLFWwindow *operator*() const { return m_window; }
    std::uint32_t width() const { return m_width; }
    std::uint32_t height() const { return m_height; }
};
