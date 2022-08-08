#include <vull/core/Window.hh>

#include <vull/core/Input.hh>
#include <vull/core/Log.hh>
#include <vull/maths/Common.hh>
#include <vull/maths/Vec.hh>
#include <vull/support/Array.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Function.hh>
#include <vull/support/HashMap.hh>
#include <vull/support/Optional.hh>
#include <vull/support/Utility.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/Swapchain.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stdlib.h>
#include <string.h>
#include <xcb/randr.h>
#include <xcb/xcb_aux.h>
#include <xcb/xproto.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wkeyword-macro"
#define explicit _explicit
#include <xcb/xkb.h>
#undef explicit
#pragma clang diagnostic pop

namespace vull {

Window::Window(uint16_t width, uint16_t height, bool fullscreen) : m_width(width), m_height(height) {
    // Open an X connection.
    m_connection = xcb_connect(nullptr, nullptr);
    VULL_ENSURE(xcb_connection_has_error(m_connection) == 0, "Failed to create X connection");

    // Create a window on the first screen and set the title.
    m_id = xcb_generate_id(m_connection);
    const uint32_t event_mask = XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE | XCB_EVENT_MASK_POINTER_MOTION |
                                XCB_EVENT_MASK_BUTTON_MOTION | XCB_EVENT_MASK_BUTTON_PRESS |
                                XCB_EVENT_MASK_BUTTON_RELEASE;
    xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(m_connection)).data;
    xcb_create_window(m_connection, screen->root_depth, m_id, screen->root, 0, 0, static_cast<uint16_t>(width),
                      static_cast<uint16_t>(height), 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual,
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

    auto use_xkb_request = xcb_xkb_use_extension(m_connection, XCB_XKB_MAJOR_VERSION, XCB_XKB_MINOR_VERSION);
    free(xcb_xkb_use_extension_reply(m_connection, use_xkb_request, nullptr));
    map_keycodes();

    // TODO: Disable auto repeat.

    // Make the window visible and wait for the server to process the requests.
    xcb_map_window(m_connection, m_id);
    xcb_aux_sync(m_connection);

    // Use the RandR extension to calculate the display pixels per centimetre.
    auto primary_output_request = xcb_randr_get_output_primary(m_connection, m_id);
    auto *primary_output = xcb_randr_get_output_primary_reply(m_connection, primary_output_request, nullptr);

    auto output_info_request = xcb_randr_get_output_info(m_connection, primary_output->output, 0);
    auto *output_info = xcb_randr_get_output_info_reply(m_connection, output_info_request, nullptr);

    auto width_mm = static_cast<float>(output_info->mm_width);
    auto height_mm = static_cast<float>(output_info->mm_height);
    auto diag_cm = sqrt(width_mm * width_mm + height_mm * height_mm) / 10.0f;
    auto diag_px = sqrt(static_cast<float>(width) * static_cast<float>(width) +
                        static_cast<float>(height) * static_cast<float>(height));
    m_ppcm = diag_px / diag_cm;

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

void Window::map_keycodes() {
    struct KeyPair {
        const char *name;
        Key key;
    };
    const Array key_pairs{
        KeyPair{"AD01", Key::Q}, KeyPair{"AD02", Key::W}, KeyPair{"AD03", Key::E},     KeyPair{"AD04", Key::R},
        KeyPair{"AD05", Key::T}, KeyPair{"AD06", Key::Y}, KeyPair{"AD07", Key::U},     KeyPair{"AD08", Key::I},
        KeyPair{"AD09", Key::O}, KeyPair{"AD10", Key::P}, KeyPair{"AC01", Key::A},     KeyPair{"AC02", Key::S},
        KeyPair{"AC03", Key::D}, KeyPair{"AC04", Key::F}, KeyPair{"AC05", Key::G},     KeyPair{"AC06", Key::H},
        KeyPair{"AC07", Key::J}, KeyPair{"AC08", Key::K}, KeyPair{"AC09", Key::L},     KeyPair{"AB01", Key::Z},
        KeyPair{"AB02", Key::X}, KeyPair{"AB03", Key::C}, KeyPair{"AB04", Key::V},     KeyPair{"AB05", Key::B},
        KeyPair{"AB06", Key::N}, KeyPair{"AB07", Key::M}, KeyPair{"SPCE", Key::Space}, KeyPair{"LFSH", Key::Shift},
    };

    auto get_names_request = xcb_xkb_get_names(m_connection, XCB_XKB_ID_USE_CORE_KBD, XCB_XKB_NAME_DETAIL_KEY_NAMES);
    auto *get_names_reply = xcb_xkb_get_names_reply(m_connection, get_names_request, nullptr);
    VULL_ENSURE(get_names_reply != nullptr);

    auto *names_value_buffer = xcb_xkb_get_names_value_list(get_names_reply);
    xcb_xkb_get_names_value_list_t names_value_list;
    xcb_xkb_get_names_value_list_unpack(names_value_buffer, get_names_reply->nTypes, get_names_reply->indicators,
                                        get_names_reply->virtualMods, get_names_reply->groupNames,
                                        get_names_reply->nKeys, get_names_reply->nKeyAliases,
                                        get_names_reply->nRadioGroups, get_names_reply->which, &names_value_list);

    auto key_name_it = xcb_xkb_get_names_value_list_key_names_iterator(get_names_reply, &names_value_list);
    for (uint32_t keycode = get_names_reply->minKeyCode; key_name_it.rem > 0; keycode++) {
        for (const auto &pair : key_pairs) {
            if (strncmp(static_cast<const char *>(key_name_it.data->name), pair.name, 4) == 0) {
                m_keycode_map[keycode] = pair.key;
            }
        }
        xcb_xkb_key_name_next(&key_name_it);
    }
    free(get_names_reply);
}

Key Window::translate_keycode(uint8_t keycode) {
    return keycode < m_keycode_map.size() ? m_keycode_map[keycode] : Key::Unknown;
}

vk::Swapchain Window::create_swapchain(const vk::Context &context, vk::SwapchainMode mode) {
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

static ButtonMask translate_button(uint8_t button) {
    switch (button) {
    case XCB_BUTTON_INDEX_1:
        return ButtonMask::Left;
    case XCB_BUTTON_INDEX_2:
        return ButtonMask::Middle;
    case XCB_BUTTON_INDEX_3:
        return ButtonMask::Right;
    default:
        return ButtonMask::None;
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
            const auto key = translate_keycode(key_event->detail);
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
            m_buttons ^= (-ButtonMask(event_id == XCB_BUTTON_PRESS) ^ m_buttons) & button;

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
            m_mouse_x = motion_event->event_x;
            m_mouse_y = motion_event->event_y;

            if (m_cursor_hidden) {
                // TODO: Delta should still work when cursor visible.
                delta_x += motion_event->event_x - static_cast<int16_t>(m_width / 2);  // NOLINT
                delta_y += motion_event->event_y - static_cast<int16_t>(m_height / 2); // NOLINT
                if (motion_event->event_x != m_width / 2 || motion_event->event_y != m_height / 2) {
                    xcb_warp_pointer(m_connection, m_id, m_id, 0, 0, m_width, m_height,
                                     static_cast<int16_t>(m_width / 2), static_cast<int16_t>(m_height / 2));
                }
            }
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

bool Window::is_button_pressed(Button button) const {
    return (m_buttons & button) != ButtonMask::None;
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

void Window::on_mouse_press(Button button, Function<MouseCallback> &&callback) {
    m_mouse_press_callbacks.set(button, vull::move(callback));
}

void Window::on_mouse_release(Button button, Function<MouseCallback> &&callback) {
    m_mouse_release_callbacks.set(button, vull::move(callback));
}

void Window::on_mouse_move(Function<MouseMoveCallback> &&callback) {
    m_mouse_move_callback = vull::move(callback);
}

} // namespace vull
