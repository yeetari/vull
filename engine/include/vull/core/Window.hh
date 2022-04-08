#pragma once

#include <vull/support/Array.hh>
#include <vull/vulkan/Swapchain.hh>

#include <stdint.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

namespace vull {

class VkContext;

enum class Key : uint8_t {
    Unknown = 0,
    // clang-format off
    A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    // clang-format on
    Shift,
    Count,
};

class Window {
    const uint16_t m_width;
    const uint16_t m_height;
    xcb_connection_t *m_connection;
    xcb_intern_atom_reply_t *m_delete_window_atom{nullptr};
    uint32_t m_id{0};
    float m_ppcm{0.0f};

    int16_t m_delta_x{0};
    int16_t m_delta_y{0};
    Array<Key, 256> m_keycode_map{};
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

    Swapchain create_swapchain(const VkContext &context);
    void close();
    void poll_events();

    bool is_key_down(Key key) const { return m_keys[static_cast<uint8_t>(key)]; }
    float delta_x() const { return static_cast<float>(m_delta_x); }
    float delta_y() const { return static_cast<float>(m_delta_y); }

    float aspect_ratio() const { return static_cast<float>(m_width) / static_cast<float>(m_height); }
    float ppcm() const { return m_ppcm; }
    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }
    bool should_close() const { return m_should_close; }
};

} // namespace vull
