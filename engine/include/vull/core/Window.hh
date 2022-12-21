#pragma once

#include <vull/core/Input.hh>
#include <vull/support/Array.hh>
#include <vull/support/Function.hh>
#include <vull/support/HashMap.hh>
#include <vull/vulkan/Swapchain.hh>

#include <stdint.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

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
    float m_ppcm{0.0f};
    Array<Key, 256> m_keycode_map{};

    uint32_t m_hidden_cursor{0};
    int16_t m_mouse_x{0};
    int16_t m_mouse_y{0};
    bool m_cursor_hidden{true};

    HashMap<Key, Function<KeyCallback>> m_key_press_callbacks;
    HashMap<Key, Function<KeyCallback>> m_key_release_callbacks;
    HashMap<Button, Function<MouseCallback>> m_mouse_press_callbacks;
    HashMap<Button, Function<MouseCallback>> m_mouse_release_callbacks;
    Function<MouseMoveCallback> m_mouse_move_callback;

    ButtonMask m_buttons;
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
    bool is_button_pressed(Button button) const;
    bool is_key_pressed(Key key) const;

    void on_key_press(Key key, Function<KeyCallback> &&callback);
    void on_key_release(Key key, Function<KeyCallback> &&callback);
    void on_mouse_press(Button button, Function<MouseCallback> &&callback);
    void on_mouse_release(Button button, Function<MouseCallback> &&callback);
    void on_mouse_move(Function<MouseMoveCallback> &&callback);

    float aspect_ratio() const { return static_cast<float>(m_width) / static_cast<float>(m_height); }
    float ppcm() const { return m_ppcm; }
    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }
    bool should_close() const { return m_should_close; }
};

} // namespace vull
