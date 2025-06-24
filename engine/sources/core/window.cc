#include <vull/core/window.hh>

#include <vull/container/array.hh>
#include <vull/container/hash_map.hh>
#include <vull/core/input.hh>
#include <vull/core/log.hh>
#include <vull/maths/vec.hh>
#include <vull/support/assert.hh>
#include <vull/support/function.hh>
#include <vull/support/optional.hh>
#include <vull/support/utility.hh>
#include <vull/vulkan/context.hh>
#include <vull/vulkan/swapchain.hh>
#include <vull/vulkan/vulkan.hh>

#include <stdlib.h>
#include <xcb/randr.h>
#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>
#include <xcb/xproto.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon-x11.h>
#include <xkbcommon/xkbcommon.h>

namespace vull {

Window::Window(Optional<uint16_t> width, Optional<uint16_t> height, bool fullscreen) {
    // Open an X connection.
    m_connection = xcb_connect(nullptr, nullptr);
    VULL_ENSURE(xcb_connection_has_error(m_connection) == 0, "Failed to create X connection");

    // Create a window on the first screen and set the title.
    m_id = xcb_generate_id(m_connection);
    const uint32_t event_mask = XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE | XCB_EVENT_MASK_POINTER_MOTION |
                                XCB_EVENT_MASK_BUTTON_MOTION | XCB_EVENT_MASK_BUTTON_PRESS |
                                XCB_EVENT_MASK_BUTTON_RELEASE;
    xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(m_connection)).data;

    m_width = width.value_or(screen->width_in_pixels);
    m_height = height.value_or(screen->height_in_pixels);

    xcb_create_window(m_connection, screen->root_depth, m_id, screen->root, 0, 0, static_cast<uint16_t>(m_width),
                      static_cast<uint16_t>(m_height), 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual,
                      XCB_CW_EVENT_MASK, &event_mask);
    xcb_change_property(m_connection, XCB_PROP_MODE_REPLACE, m_id, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, 4, "vull");

    // Set up the delete window protocol via the WM_PROTOCOLS property.
    auto protocols_atom_request = xcb_intern_atom(m_connection, 1, 12, "WM_PROTOCOLS");
    auto *protocols_atom = xcb_intern_atom_reply(m_connection, protocols_atom_request, nullptr);
    auto delete_window_atom_request = xcb_intern_atom(m_connection, 0, 16, "WM_DELETE_WINDOW");
    m_delete_window_atom = xcb_intern_atom_reply(m_connection, delete_window_atom_request, nullptr);
    xcb_change_property(m_connection, XCB_PROP_MODE_REPLACE, m_id, protocols_atom->atom, XCB_ATOM_ATOM, 32, 1,
                        &m_delete_window_atom->atom);
    free(protocols_atom);

    if (fullscreen) {
        auto wm_state_atom_request = xcb_intern_atom(m_connection, 0, 13, "_NET_WM_STATE");
        auto *wm_state_atom = xcb_intern_atom_reply(m_connection, wm_state_atom_request, nullptr);
        auto wm_fullscreen_atom_request = xcb_intern_atom(m_connection, 0, 24, "_NET_WM_STATE_FULLSCREEN");
        auto *wm_fullscreen_atom = xcb_intern_atom_reply(m_connection, wm_fullscreen_atom_request, nullptr);
        xcb_change_property(m_connection, XCB_PROP_MODE_REPLACE, m_id, wm_state_atom->atom, XCB_ATOM_ATOM, 32, 1,
                            &wm_fullscreen_atom->atom);
        free(wm_fullscreen_atom);
        free(wm_state_atom);
    }

    // Hide cursor.
    m_hidden_cursor = xcb_generate_id(m_connection);
    xcb_pixmap_t cursor_pixmap = xcb_generate_id(m_connection);
    xcb_create_pixmap(m_connection, 1, cursor_pixmap, m_id, 1, 1);
    xcb_create_cursor(m_connection, m_hidden_cursor, cursor_pixmap, cursor_pixmap, 0, 0, 0, 0, 0, 0, 0, 0);
    xcb_free_pixmap(m_connection, cursor_pixmap);
    xcb_change_window_attributes(m_connection, m_id, XCB_CW_CURSOR, &m_hidden_cursor);

    xkb_x11_setup_xkb_extension(m_connection, XKB_X11_MIN_MAJOR_XKB_VERSION, XKB_X11_MIN_MINOR_XKB_VERSION,
                                XKB_X11_SETUP_XKB_EXTENSION_NO_FLAGS, nullptr, nullptr, nullptr, nullptr);
    setup_xkb();

    // TODO: Disable auto repeat.

    // Make the window visible and wait for the server to process the requests.
    xcb_map_window(m_connection, m_id);
    xcb_aux_sync(m_connection);

    // Use the RandR extension to calculate the display pixels per centimetre.
    auto primary_output_request = xcb_randr_get_output_primary(m_connection, m_id);
    auto *primary_output = xcb_randr_get_output_primary_reply(m_connection, primary_output_request, nullptr);

    auto output_info_request = xcb_randr_get_output_info(m_connection, primary_output->output, 0);
    auto *output_info = xcb_randr_get_output_info_reply(m_connection, output_info_request, nullptr);

    auto width_cm = static_cast<float>(output_info->mm_width) / 10.0f;
    auto height_cm = width_cm / aspect_ratio();
    m_ppcm = {static_cast<float>(m_width) / width_cm, static_cast<float>(m_height) / height_cm};

    free(primary_output);
    free(output_info);

    // Center the mouse pointer.
    xcb_warp_pointer(m_connection, m_id, m_id, 0, 0, m_width, m_height, static_cast<int16_t>(m_width / 2),
                     static_cast<int16_t>(m_height / 2));
}

Window::~Window() {
    free(m_delete_window_atom);
    xcb_destroy_window(m_connection, m_id);
    xcb_disconnect(m_connection);
}

void Window::setup_xkb() {
    auto *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    int32_t device_id = xkb_x11_get_core_keyboard_device_id(m_connection);
    auto *keymap = xkb_x11_keymap_new_from_device(context, m_connection, device_id, XKB_KEYMAP_COMPILE_NO_FLAGS);
    m_xkb_state = xkb_x11_state_new_from_device(keymap, m_connection, device_id);
}

vk::Swapchain Window::create_swapchain(vk::Context &context, vk::SwapchainMode mode) {
    vkb::XcbSurfaceCreateInfoKHR surface_ci{
        .sType = vkb::StructureType::XcbSurfaceCreateInfoKHR,
        .connection = m_connection,
        .window = m_id,
    };
    vkb::SurfaceKHR surface;
    context.vkCreateXcbSurfaceKHR(&surface_ci, &surface);
    return {context, {m_width, m_height}, surface, mode};
}

void Window::close() {
    m_should_close = true;
}

void Window::hide_cursor() {
    m_cursor_hidden = true;
    xcb_change_window_attributes(m_connection, m_id, XCB_CW_CURSOR, &m_hidden_cursor);
    xcb_warp_pointer(m_connection, m_id, m_id, 0, 0, m_width, m_height, static_cast<int16_t>(m_width / 2),
                     static_cast<int16_t>(m_height / 2));
}

void Window::show_cursor() {
    m_cursor_hidden = false;
    uint32_t cursor = 0;
    xcb_change_window_attributes(m_connection, m_id, XCB_CW_CURSOR, &cursor);
}

static MouseButtonMask translate_button(uint8_t button) {
    switch (button) {
    case XCB_BUTTON_INDEX_1:
        return MouseButtonMask::Left;
    case XCB_BUTTON_INDEX_2:
        return MouseButtonMask::Middle;
    case XCB_BUTTON_INDEX_3:
        return MouseButtonMask::Right;
    default:
        return MouseButtonMask::None;
    }
}

static Key translate_key(xkb_keysym_t keysym) {
    switch (keysym) {
    case XKB_KEY_F1:
        return Key::F1;
    case XKB_KEY_F2:
        return Key::F2;
    case XKB_KEY_F3:
        return Key::F3;
    case XKB_KEY_F4:
        return Key::F4;
    case XKB_KEY_F5:
        return Key::F5;
    case XKB_KEY_F6:
        return Key::F6;
    case XKB_KEY_F7:
        return Key::F7;
    case XKB_KEY_F8:
        return Key::F8;
    case XKB_KEY_F9:
        return Key::F9;
    case XKB_KEY_F10:
        return Key::F10;
    case XKB_KEY_F11:
        return Key::F11;
    case XKB_KEY_F12:
        return Key::F12;
    case XKB_KEY_a:
        return Key::A;
    case XKB_KEY_b:
        return Key::B;
    case XKB_KEY_c:
        return Key::C;
    case XKB_KEY_d:
        return Key::D;
    case XKB_KEY_e:
        return Key::E;
    case XKB_KEY_f:
        return Key::F;
    case XKB_KEY_g:
        return Key::G;
    case XKB_KEY_h:
        return Key::H;
    case XKB_KEY_i:
        return Key::I;
    case XKB_KEY_j:
        return Key::J;
    case XKB_KEY_k:
        return Key::K;
    case XKB_KEY_l:
        return Key::L;
    case XKB_KEY_m:
        return Key::M;
    case XKB_KEY_n:
        return Key::N;
    case XKB_KEY_o:
        return Key::O;
    case XKB_KEY_p:
        return Key::P;
    case XKB_KEY_q:
        return Key::Q;
    case XKB_KEY_r:
        return Key::R;
    case XKB_KEY_s:
        return Key::S;
    case XKB_KEY_t:
        return Key::T;
    case XKB_KEY_u:
        return Key::U;
    case XKB_KEY_v:
        return Key::V;
    case XKB_KEY_w:
        return Key::W;
    case XKB_KEY_x:
        return Key::X;
    case XKB_KEY_y:
        return Key::Y;
    case XKB_KEY_z:
        return Key::Z;
    case XKB_KEY_space:
        return Key::Space;
    case XKB_KEY_Shift_L:
        return Key::Shift;
    default:
        return Key::Unknown;
    }
}

static ModifierMask translate_mods(uint16_t state) {
    auto mask = static_cast<ModifierMask>(0);
    if ((state & XCB_MOD_MASK_SHIFT) != 0u) {
        mask |= ModifierMask::Shift;
    }
    if ((state & XCB_MOD_MASK_CONTROL) != 0u) {
        mask |= ModifierMask::Ctrl;
    }
    if ((state & XCB_MOD_MASK_1) != 0u) {
        mask |= ModifierMask::Alt;
    }
    if ((state & XCB_MOD_MASK_4) != 0u) {
        mask |= ModifierMask::Super;
    }
    if ((state & XCB_MOD_MASK_LOCK) != 0u) {
        mask |= ModifierMask::CapsLock;
    }
    return mask;
}

void Window::poll_events() {
    int16_t delta_x = 0;
    int16_t delta_y = 0;
    bool had_mouse_move = false;

    xcb_generic_event_t *event;
    while ((event = xcb_poll_for_event(m_connection)) != nullptr) {
        const auto event_id = event->response_type & ~0x80u;
        switch (event_id) {
        case XCB_KEY_PRESS:
        case XCB_KEY_RELEASE: {
            const auto *key_event = reinterpret_cast<xcb_key_press_event_t *>(event);
            const auto keysym = xkb_state_key_get_one_sym(m_xkb_state, key_event->detail);
            const auto key = translate_key(keysym);

            m_keys[static_cast<uint8_t>(key)] = event_id == XCB_KEY_PRESS;

            const auto mods = translate_mods(key_event->state);
            if (event_id == XCB_KEY_PRESS && m_key_press_callbacks.contains(key)) {
                if (auto &callback = *m_key_press_callbacks.get(key)) {
                    callback(mods);
                }
            } else if (event_id == XCB_KEY_RELEASE && m_key_release_callbacks.contains(key)) {
                if (auto &callback = *m_key_release_callbacks.get(key)) {
                    callback(mods);
                }
            }
            break;
        }
        case XCB_BUTTON_PRESS:
        case XCB_BUTTON_RELEASE: {
            const auto *mouse_event = reinterpret_cast<xcb_button_press_event_t *>(event);
            const auto button = translate_button(mouse_event->detail);
            m_buttons ^= (-MouseButtonMask(event_id == XCB_BUTTON_PRESS) ^ m_buttons) & button;

            Vec2f position(m_mouse_x, m_mouse_y);
            if (event_id == XCB_BUTTON_PRESS && m_mouse_press_callbacks.contains(button)) {
                if (auto &callback = *m_mouse_press_callbacks.get(button)) {
                    callback(position);
                }
            } else if (event_id == XCB_BUTTON_RELEASE && m_mouse_release_callbacks.contains(button)) {
                if (auto &callback = *m_mouse_release_callbacks.get(button)) {
                    callback(position);
                }
            }
            break;
        }
        case XCB_MOTION_NOTIFY: {
            const auto *motion_event = reinterpret_cast<xcb_motion_notify_event_t *>(event);
            had_mouse_move = true;
            if (m_cursor_hidden) {
                // TODO: Don't do this.
                delta_x += motion_event->event_x - static_cast<int16_t>(m_width / 2);  // NOLINT
                delta_y += motion_event->event_y - static_cast<int16_t>(m_height / 2); // NOLINT
                if (motion_event->event_x != m_width / 2 || motion_event->event_y != m_height / 2) {
                    xcb_warp_pointer(m_connection, m_id, m_id, 0, 0, m_width, m_height,
                                     static_cast<int16_t>(m_width / 2), static_cast<int16_t>(m_height / 2));
                }
            } else {
                delta_x += motion_event->event_x - m_mouse_x; // NOLINT
                delta_y += motion_event->event_y - m_mouse_y; // NOLINT
            }
            m_mouse_x = motion_event->event_x;
            m_mouse_y = motion_event->event_y;
            break;
        }
        case XCB_CLIENT_MESSAGE: {
            const auto &data = reinterpret_cast<xcb_client_message_event_t *>(event)->data;
            if (data.data32[0] == m_delete_window_atom->atom) {
                m_should_close = true;
            }
            break;
        }
        default:
            vull::trace("[core] Received unknown X event {}", event_id);
            break;
        }
        free(event);
    }
    xcb_flush(m_connection);

    if (had_mouse_move && m_mouse_move_callback) {
        Vec2f delta(delta_x, delta_y);
        Vec2f position(m_mouse_x, m_mouse_y);
        m_mouse_move_callback(delta, position, m_buttons);
    }
}

bool Window::is_button_pressed(MouseButton button) const {
    return (m_buttons & button) != MouseButtonMask::None;
}

bool Window::is_key_pressed(Key key) const {
    return m_keys[static_cast<uint8_t>(key)];
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

} // namespace vull
