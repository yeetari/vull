#pragma once

#include <vull/container/hash_map.hh>
#include <vull/core/input.hh>
#include <vull/maths/vec.hh>
#include <vull/support/function.hh>
#include <vull/support/optional.hh>
#include <vull/support/result.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/vulkan/context.hh>
#include <vull/vulkan/swapchain.hh>
#include <vull/vulkan/vulkan.hh>

namespace vull {

enum class WindowError {
    Unsupported,
    ConnectionFailed,
    XkbError,
    XkbUnsupported,
    XInputError,
    XInputUnsupported,
};

using WindowCloseCallback = void();

class Window {
protected:
    Vec2u m_resolution;
    Vec2f m_ppcm;
    Function<WindowCloseCallback> m_close_callback;

    // TODO: Make an InputSystem class.
    int16_t m_mouse_x{0};
    int16_t m_mouse_y{0};
    bool m_cursor_grabbed{true};
    HashMap<Key, Function<KeyCallback>> m_key_press_callbacks;
    HashMap<Key, Function<KeyCallback>> m_key_release_callbacks;
    HashMap<MouseButton, Function<MouseCallback>> m_mouse_press_callbacks;
    HashMap<MouseButton, Function<MouseCallback>> m_mouse_release_callbacks;
    Function<MouseMoveCallback> m_mouse_move_callback;
    MouseButtonMask m_buttons;
    Array<bool, static_cast<uint8_t>(Key::Count)> m_keys{};

    Window(Vec2u resolution, Vec2f ppcm) : m_resolution(resolution), m_ppcm(ppcm) {}

public:
    static Result<UniquePtr<Window>, WindowError> create(Optional<uint16_t> width, Optional<uint16_t> height,
                                                         bool fullscreen);
    static Result<UniquePtr<Window>, WindowError> create_x11(Optional<uint16_t> width, Optional<uint16_t> height,
                                                             bool fullscreen);

    Window(const Window &) = delete;
    Window(Window &&) = delete;
    virtual ~Window() = default;

    Window &operator=(const Window &) = delete;
    Window &operator=(Window &&) = delete;

    virtual Result<vk::Swapchain, vkb::Result> create_swapchain(vk::Context &context, vk::SwapchainMode mode) = 0;
    virtual void poll_events() = 0;

    bool is_button_pressed(MouseButton button) const;
    bool is_key_pressed(Key key) const;

    void on_close(Function<WindowCloseCallback> &&callback);
    void on_key_press(Key key, Function<KeyCallback> &&callback);
    void on_key_release(Key key, Function<KeyCallback> &&callback);
    void on_mouse_press(MouseButton button, Function<MouseCallback> &&callback);
    void on_mouse_release(MouseButton button, Function<MouseCallback> &&callback);
    void on_mouse_move(Function<MouseMoveCallback> &&callback);

    virtual void grab_cursor() = 0;
    virtual void ungrab_cursor() = 0;
    bool cursor_grabbed() const { return m_cursor_grabbed; }

    float aspect_ratio() const;
    Vec2u resolution() const { return m_resolution; }
    Vec2f ppcm() const { return m_ppcm; }
};

inline float Window::aspect_ratio() const {
    return static_cast<float>(m_resolution.x()) / static_cast<float>(m_resolution.y());
}

} // namespace vull
