#pragma once

struct GLFWwindow;

class Window {
    const int m_width;
    const int m_height;
    GLFWwindow *m_window;

public:
    static void poll_events();

    Window(int width, int height);
    Window(const Window &) = delete;
    Window(Window &&) = delete;
    ~Window();

    Window &operator=(const Window &) = delete;
    Window &operator=(Window &&) = delete;

    float aspect_ratio() const;
    bool should_close() const;

    GLFWwindow *operator*() const { return m_window; }
};
