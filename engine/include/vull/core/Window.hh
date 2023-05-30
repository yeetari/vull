#pragma once

#include <vull/container/Array.hh>
#include <vull/container/HashMap.hh>
#include <vull/core/Input.hh>
#include <vull/maths/Vec.hh>
#include <vull/support/Function.hh>
#include <vull/vulkan/Swapchain.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stdint.h>
// IWYU pragma: no_include <xcb/xproto.h>

using xcb_intern_atom_reply_t = struct xcb_intern_atom_reply_t;

namespace vull::vk {

class Context;

} // namespace vull::vk

namespace vull {

class Window {
    const uint16_t m_width;
    const uint16_t m_height;
    xcb_connection_t *m_connection;
    xcb_intern_atom_reply_t *m_delete_window_atom{nullptr};
    uint32_t m_id{0};
    Vec2f m_ppcm;
    Array<Key, 256> m_keycode_map{};

    uint32_t m_hidden_cursor{0};
    int16_t m_mouse_x{0};
    int16_t m_mouse_y{0};
    bool m_cursor_hidden{true};

    HashMap<Key, Function<KeyCallback>> m_key_press_callbacks;
    HashMap<Key, Function<KeyCallback>> m_key_release_callbacks;
    HashMap<MouseButton, Function<MouseCallback>> m_mouse_press_callbacks;
    HashMap<MouseButton, Function<MouseCallback>> m_mouse_release_callbacks;
    Function<MouseMoveCallback> m_mouse_move_callback;

    MouseButtonMask m_buttons;
    Array<bool, static_cast<uint8_t>(Key::Count)> m_keys{};
    bool m_should_close{false};

    void map_keycodes();
    Key translate_keycode(uint8_t keycode);

public:
    Window(uint16_t width, uint16_t height, bool fullscreen);
    Window(const Window &) = delete;
    Window(Window &&) = delete;
    ~Window();

    Window &operator=(const Window &) = delete;
    Window &operator=(Window &&) = delete;

    vk::Swapchain create_swapchain(vk::Context &context, vk::SwapchainMode mode);
    void close();
    void hide_cursor();
    void show_cursor();
    void poll_events();
    bool is_button_pressed(MouseButton button) const;
    bool is_key_pressed(Key key) const;

    void on_key_press(Key key, Function<KeyCallback> &&callback);
    void on_key_release(Key key, Function<KeyCallback> &&callback);
    void on_mouse_press(MouseButton button, Function<MouseCallback> &&callback);
    void on_mouse_release(MouseButton button, Function<MouseCallback> &&callback);
    void on_mouse_move(Function<MouseMoveCallback> &&callback);

    float aspect_ratio() const { return static_cast<float>(m_width) / static_cast<float>(m_height); }
    Vec2f ppcm() const { return m_ppcm; }
    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }
    Vec2u mouse_position() const { return {m_mouse_x, m_mouse_y}; }
    bool cursor_hidden() const { return m_cursor_hidden; }
    bool should_close() const { return m_should_close; }
};

} // namespace vull
