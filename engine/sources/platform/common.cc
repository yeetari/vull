#include <vull/platform/window.hh>

#include <vull/container/array.hh>
#include <vull/container/hash_map.hh>
#include <vull/core/input.hh>
#include <vull/core/log.hh> // IWYU pragma: keep
#include <vull/support/function.hh>
#include <vull/support/optional.hh>
#include <vull/support/result.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>

#include <stdint.h>

namespace vull::platform {

Result<UniquePtr<Window>, WindowError> Window::create(Optional<uint16_t> width, Optional<uint16_t> height) {
#ifdef VULL_BUILD_WAYLAND_WINDOW
    vull::trace("[window] Attempting to create Wayland window");
    if (auto wayland = create_wayland(width, height); !wayland.is_error()) {
        return wayland;
    }
#endif
#ifdef VULL_BUILD_X11_WINDOW
    vull::trace("[window] Attempting to create X11 window");
    if (auto x11 = create_x11(width, height); !x11.is_error()) {
        return x11;
    }
#endif
    return WindowError::Unsupported;
}

bool Window::is_button_pressed(MouseButton button) const {
    return (m_buttons & button) != MouseButtonMask::None;
}

bool Window::is_key_pressed(Key key) const {
    return m_keys[static_cast<uint8_t>(key)];
}

void Window::on_close(Function<WindowCloseCallback> &&callback) {
    m_close_callback = vull::move(callback);
}

void Window::on_key_press(Key key, Function<KeyCallback> &&callback) {
    m_key_press_callbacks.set(key, vull::move(callback));
}

void Window::on_key_release(Key key, Function<KeyCallback> &&callback) {
    m_key_release_callbacks.set(key, vull::move(callback));
}

void Window::on_mouse_press(MouseButton button, Function<MouseCallback> &&callback) {
    m_mouse_press_callbacks.set(button, vull::move(callback));
}

void Window::on_mouse_release(MouseButton button, Function<MouseCallback> &&callback) {
    m_mouse_release_callbacks.set(button, vull::move(callback));
}

void Window::on_mouse_move(Function<MouseMoveCallback> &&callback) {
    m_mouse_move_callback = vull::move(callback);
}

} // namespace vull::platform
