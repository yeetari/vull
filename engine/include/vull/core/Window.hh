#pragma once

#include <vull/vulkan/Swapchain.hh>

#include <stdint.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

namespace vull {

class Context;

class Window {
    const uint32_t m_width;
    const uint32_t m_height;
    xcb_connection_t *m_connection;
    xcb_intern_atom_reply_t *m_delete_window_atom{nullptr};
    uint32_t m_id{0};
    bool m_should_close{false};

public:
    Window(uint32_t width, uint32_t height, bool fullscreen);
    Window(const Window &) = delete;
    Window(Window &&) = delete;
    ~Window();

    Window &operator=(const Window &) = delete;
    Window &operator=(Window &&) = delete;

    Swapchain create_swapchain(const Context &context);
    void close();
    void poll_events();

    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }
    bool should_close() const { return m_should_close; }
};

} // namespace vull
