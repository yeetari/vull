#include <vull/platform/window.hh>

#include <vull/container/array.hh>
#include <vull/container/hash_map.hh>
#include <vull/core/input.hh>
#include <vull/core/log.hh>
#include <vull/maths/epsilon.hh>
#include <vull/maths/vec.hh>
#include <vull/support/enum.hh>
#include <vull/support/function.hh>
#include <vull/support/optional.hh>
#include <vull/support/result.hh>
#include <vull/support/string_view.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>
#include <vull/vulkan/context.hh>
#include <vull/vulkan/swapchain.hh>
#include <vull/vulkan/vulkan.hh>

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <xcb/randr.h>
#include <xcb/xcb.h>
#include <xcb/xinput.h>
#include <xcb/xproto.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon-x11.h>
#include <xkbcommon/xkbcommon.h>

// IWYU pragma: no_forward_declare xkb_state

namespace vull::platform {
namespace {

class WindowX11 : public Window {
    xcb_connection_t *const m_connection;
    xcb_screen_t *const m_screen;
    xcb_intern_atom_reply_t *const m_delete_window_atom{nullptr};
    xkb_state *const m_xkb_state{nullptr};
    const uint32_t m_id;
    const uint32_t m_hidden_cursor_id;
    const uint8_t m_xinput_opcode;

    void handle_event(uint8_t, const xcb_key_press_event_t *);
    void handle_event(uint8_t, const xcb_button_press_event_t *);
    void handle_event(const xcb_motion_notify_event_t *);
    void handle_event(const xcb_expose_event_t *);
    void handle_event(const xcb_client_message_event_t *);
    void handle_event(const xcb_ge_generic_event_t *);

public:
    WindowX11(Vec2u resolution, Vec2f ppcm, xcb_connection_t *connection, xcb_screen_t *screen,
              xcb_intern_atom_reply_t *delete_window_atom, xkb_state *xkb_state, uint32_t id, uint32_t hidden_cursor_id,
              uint8_t xinput_opcode)
        : Window(resolution, ppcm), m_connection(connection), m_screen(screen),
          m_delete_window_atom(delete_window_atom), m_xkb_state(xkb_state), m_id(id),
          m_hidden_cursor_id(hidden_cursor_id), m_xinput_opcode(xinput_opcode) {}
    WindowX11(const WindowX11 &) = delete;
    WindowX11(WindowX11 &&) = delete;
    ~WindowX11() override;

    WindowX11 &operator=(const WindowX11 &) = delete;
    WindowX11 &operator=(WindowX11 &&) = delete;

    Result<vk::Swapchain, vkb::Result> create_swapchain(vk::Context &context, vk::SwapchainMode mode) override;
    void poll_events() override;

    void grab_cursor() override;
    void ungrab_cursor() override;
};

WindowX11::~WindowX11() {
    xcb_free_cursor(m_connection, m_hidden_cursor_id);
    xkb_state_unref(m_xkb_state);
    free(m_delete_window_atom);
    xcb_destroy_window(m_connection, m_id);
    xcb_disconnect(m_connection);
}

Result<vk::Swapchain, vkb::Result> WindowX11::create_swapchain(vk::Context &context, vk::SwapchainMode mode) {
    vkb::XcbSurfaceCreateInfoKHR surface_ci{
        .sType = vkb::StructureType::XcbSurfaceCreateInfoKHR,
        .connection = m_connection,
        .window = m_id,
    };
    vkb::SurfaceKHR surface;
    if (auto result = context.vkCreateXcbSurfaceKHR(&surface_ci, &surface); result != vkb::Result::Success) {
        return result;
    }
    return vk::Swapchain(context, {m_resolution.x(), m_resolution.y()}, surface, mode);
}

Key translate_key(xkb_keysym_t keysym) {
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

ModifierMask translate_mods(uint16_t state) {
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

void WindowX11::handle_event(uint8_t event_id, const xcb_key_press_event_t *key_event) {
    const auto key = translate_key(xkb_state_key_get_one_sym(m_xkb_state, key_event->detail));
    m_keys[vull::to_underlying(key)] = event_id == XCB_KEY_PRESS;
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
}

MouseButtonMask translate_button(uint8_t button) {
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

void WindowX11::handle_event(uint8_t event_id, const xcb_button_press_event_t *button_event) {
    Vec2f position(m_mouse_x, m_mouse_y);
    const auto button = translate_button(button_event->detail);
    if (event_id == XCB_BUTTON_PRESS) {
        m_buttons |= button;
        if (auto callback = m_mouse_press_callbacks.get(button)) {
            (*callback)(position);
        }
    } else if (event_id == XCB_BUTTON_RELEASE) {
        m_buttons &= ~button;
        if (auto callback = m_mouse_release_callbacks.get(button)) {
            (*callback)(position);
        }
    }
}

void WindowX11::handle_event(const xcb_motion_notify_event_t *motion_event) {
    if (m_cursor_grabbed) {
        return;
    }
    const auto delta_x = motion_event->event_x - m_mouse_x;
    const auto delta_y = motion_event->event_y - m_mouse_y;
    m_mouse_x = motion_event->event_x;
    m_mouse_y = motion_event->event_y;
    if (m_mouse_move_callback) {
        Vec2f delta(delta_x, delta_y);
        Vec2f position(m_mouse_x, m_mouse_y);
        m_mouse_move_callback(delta, position, m_buttons);
    }
}

void WindowX11::handle_event(const xcb_expose_event_t *) {}

void WindowX11::handle_event(const xcb_client_message_event_t *client_message) {
    if (client_message->data.data32[0] == m_delete_window_atom->atom && m_close_callback) {
        m_close_callback();
    }
}

float fp3232_to_float(xcb_input_fp3232_t *fp3232) {
    // TODO: No idea if this is right.
    const auto whole = static_cast<float>(fp3232->integral);
    const auto fractional = static_cast<float>(fp3232->frac) / static_cast<float>(UINT32_MAX);
    return whole + copysignf(fractional, whole);
}

void WindowX11::handle_event(const xcb_ge_generic_event_t *generic_event) {
    if (!m_cursor_grabbed) {
        return;
    }
    if (generic_event->extension != m_xinput_opcode || generic_event->event_type != XCB_INPUT_RAW_MOTION) {
        return;
    }
    const auto *motion_event = vull::bit_cast<xcb_input_raw_motion_event_t *>(generic_event);
    const auto *mask = xcb_input_raw_button_press_valuator_mask(motion_event);
    if (mask == nullptr) {
        return;
    }

    Vec2f delta;
    auto iterator = xcb_input_raw_button_press_axisvalues_iterator(motion_event);
    if ((*mask & 0b1u) != 0u) {
        delta.set_x(fp3232_to_float(iterator.data));
        xcb_input_fp3232_next(&iterator);
    }
    if ((*mask & 0b10u) != 0u) {
        delta.set_y(fp3232_to_float(iterator.data));
    }

    if (!vull::fuzzy_zero(delta) && m_mouse_move_callback) {
        m_mouse_move_callback(delta, {}, m_buttons);
    }
}

void WindowX11::poll_events() {
    xcb_generic_event_t *event;
    while ((event = xcb_poll_for_event(m_connection)) != nullptr) {
        const auto event_id = static_cast<uint8_t>(event->response_type & ~0x80u);
        switch (event_id) {
        case XCB_KEY_PRESS:
        case XCB_KEY_RELEASE:
            handle_event(event_id, vull::bit_cast<xcb_key_press_event_t *>(event));
            break;
        case XCB_BUTTON_PRESS:
        case XCB_BUTTON_RELEASE:
            handle_event(event_id, vull::bit_cast<xcb_button_press_event_t *>(event));
            break;
        case XCB_MOTION_NOTIFY:
            handle_event(vull::bit_cast<xcb_motion_notify_event_t *>(event));
            break;
        case XCB_EXPOSE:
            handle_event(vull::bit_cast<xcb_expose_event_t *>(event));
            break;
        case XCB_CLIENT_MESSAGE:
            handle_event(vull::bit_cast<xcb_client_message_event_t *>(event));
            break;
        case XCB_GE_GENERIC:
            handle_event(vull::bit_cast<xcb_ge_generic_event_t *>(event));
            break;
        default:
            vull::warn("[platform] Received unknown X event {}", event_id);
            break;
        }
        free(event);
    }
    xcb_flush(m_connection);
}

void WindowX11::grab_cursor() {
    m_cursor_grabbed = true;
    xcb_grab_pointer(m_connection, 1, m_screen->root, XCB_NONE, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, m_id,
                     m_hidden_cursor_id, XCB_CURRENT_TIME);
}

void WindowX11::ungrab_cursor() {
    m_cursor_grabbed = false;
    xcb_ungrab_pointer(m_connection, XCB_CURRENT_TIME);
    xcb_warp_pointer(m_connection, m_id, m_id, 0, 0, static_cast<uint16_t>(m_resolution.x()),
                     static_cast<uint16_t>(m_resolution.y()), static_cast<int16_t>(m_resolution.x() / 2),
                     static_cast<int16_t>(m_resolution.y() / 2));
}

xcb_intern_atom_cookie_t intern_atom(xcb_connection_t *connection, StringView name) {
    return xcb_intern_atom(connection, 1, static_cast<uint16_t>(name.length()), name.data());
}

} // namespace

// TODO: Improve the error checking here.

Result<UniquePtr<Window>, WindowError> Window::create_x11(Optional<uint16_t> width, Optional<uint16_t> height,
                                                          bool fullscreen) {
    // Attempt to open a connection.
    auto *connection = xcb_connect(nullptr, nullptr);
    if (xcb_connection_has_error(connection) > 0) {
        return WindowError::ConnectionFailed;
    }

    // Get the desired resolution.
    auto *screen = xcb_setup_roots_iterator(xcb_get_setup(connection)).data;
    width = width.value_or(screen->width_in_pixels);
    height = height.value_or(screen->height_in_pixels);

    // Create a window on the first screen.
    const uint32_t event_mask = XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE | XCB_EVENT_MASK_BUTTON_PRESS |
                                XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_EXPOSURE;
    const uint32_t id = xcb_generate_id(connection);
    xcb_create_window(connection, screen->root_depth, id, screen->root, 0, 0, *width, *height, 0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual, XCB_CW_EVENT_MASK, &event_mask);

    // Get the WM_PROTOCOLS atom.
    auto protocols_atoms_request = intern_atom(connection, "WM_PROTOCOLS");
    auto *protocols_atom = xcb_intern_atom_reply(connection, protocols_atoms_request, nullptr);

    // Get the window deletion atom.
    auto delete_window_atom_request = intern_atom(connection, "WM_DELETE_WINDOW");
    auto *delete_window_atom = xcb_intern_atom_reply(connection, delete_window_atom_request, nullptr);
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, id, protocols_atom->atom, XCB_ATOM_ATOM, 32, 1,
                        &delete_window_atom->atom);
    free(protocols_atom);

    if (fullscreen) {
        auto state_atom_request = intern_atom(connection, "_NET_WM_STATE");
        auto fullscreen_atom_request = intern_atom(connection, "_NET_WM_STATE_FULLSCREEN");
        auto *state_atom = xcb_intern_atom_reply(connection, state_atom_request, nullptr);
        auto *fullscreen_atom = xcb_intern_atom_reply(connection, fullscreen_atom_request, nullptr);
        xcb_change_property(connection, XCB_PROP_MODE_REPLACE, id, state_atom->atom, XCB_ATOM_ATOM, 32, 1,
                            &fullscreen_atom->atom);
        free(fullscreen_atom);
        free(state_atom);
    }

    // Setup XKB.
    if (xkb_x11_setup_xkb_extension(connection, XKB_X11_MIN_MAJOR_XKB_VERSION, XKB_X11_MIN_MINOR_XKB_VERSION,
                                    XKB_X11_SETUP_XKB_EXTENSION_NO_FLAGS, nullptr, nullptr, nullptr, nullptr) != 1) {
        return WindowError::XkbUnsupported;
    }
    auto *xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (xkb_context == nullptr) {
        return WindowError::XkbError;
    }
    int32_t keyboard_device_id = xkb_x11_get_core_keyboard_device_id(connection);
    if (keyboard_device_id == -1) {
        xkb_context_unref(xkb_context);
        return WindowError::XkbError;
    }
    auto *keymap =
        xkb_x11_keymap_new_from_device(xkb_context, connection, keyboard_device_id, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (keymap == nullptr) {
        xkb_context_unref(xkb_context);
        return WindowError::XkbError;
    }
    auto *xkb_state = xkb_x11_state_new_from_device(keymap, connection, keyboard_device_id);
    xkb_keymap_unref(keymap);
    xkb_context_unref(xkb_context);
    if (xkb_state == nullptr) {
        return WindowError::XkbError;
    }

    // Setup xinput.
    const auto *xinput = xcb_get_extension_data(connection, &xcb_input_id);
    if (xinput->present != 1) {
        return WindowError::XkbUnsupported;
    }
    struct {
        xcb_input_event_mask_t head{
            .deviceid = XCB_INPUT_DEVICE_ALL_MASTER,
            .mask_len = 1,
        };
        xcb_input_xi_event_mask_t mask{
            XCB_INPUT_XI_EVENT_MASK_RAW_MOTION,
        };
    } mask;
    xcb_input_xi_select_events(connection, screen->root, 1, &mask.head);

    // Create hidden cursor.
    const uint32_t hidden_cursor_id = xcb_generate_id(connection);
    const uint32_t hidden_pixmap_id = xcb_generate_id(connection);
    xcb_create_pixmap(connection, 1, hidden_pixmap_id, id, 1, 1);
    xcb_create_cursor(connection, hidden_cursor_id, hidden_pixmap_id, hidden_pixmap_id, 0, 0, 0, 0, 0, 0, 0, 0);
    xcb_free_pixmap(connection, hidden_pixmap_id);

    // Calculate the ppcm with RandR.
    Vec2f resolution_float(*width, *height);
    auto primary_output_request = xcb_randr_get_output_primary(connection, id);
    auto *primary_output = xcb_randr_get_output_primary_reply(connection, primary_output_request, nullptr);
    auto output_info_request = xcb_randr_get_output_info(connection, primary_output->output, 0);
    auto *output_info = xcb_randr_get_output_info_reply(connection, output_info_request, nullptr);
    auto width_cm = static_cast<float>(output_info->mm_width) / 10.0f;
    auto height_cm = width_cm / (resolution_float.x() / resolution_float.y());
    Vec2f ppcm = resolution_float / Vec2f(width_cm, height_cm);
    free(output_info);
    free(primary_output);

    // Make the window visible and sync the connection.
    xcb_map_window(connection, id);
    free(xcb_get_input_focus_reply(connection, xcb_get_input_focus(connection), nullptr));

    Vec2u resolution(*width, *height);
    return vull::make_unique<WindowX11>(resolution, ppcm, connection, screen, delete_window_atom, xkb_state, id,
                                        hidden_cursor_id, xinput->major_opcode);
}

} // namespace vull::platform
