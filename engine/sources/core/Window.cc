#include <vull/core/Window.hh>

#include <vull/support/Array.hh>
#include <vull/support/Assert.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/Swapchain.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stdlib.h>
#include <string.h>
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
    const uint32_t event_mask = XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE | XCB_EVENT_MASK_POINTER_MOTION;
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

        xcb_pixmap_t cursor_pixmap = xcb_generate_id(m_connection);
        xcb_create_pixmap(m_connection, 1, cursor_pixmap, m_id, 1, 1);

        const uint32_t cursor_value = xcb_generate_id(m_connection);
        xcb_create_cursor(m_connection, cursor_value, cursor_pixmap, cursor_pixmap, 0, 0, 0, 0, 0, 0, 0, 0);
        xcb_free_pixmap(m_connection, cursor_pixmap);
        xcb_change_window_attributes(m_connection, m_id, XCB_CW_CURSOR, &cursor_value);
    }

    auto use_xkb_request = xcb_xkb_use_extension(m_connection, XCB_XKB_MAJOR_VERSION, XCB_XKB_MINOR_VERSION);
    free(xcb_xkb_use_extension_reply(m_connection, use_xkb_request, nullptr));
    map_keycodes();

    // Make the window visible and wait for the server to process the requests.
    xcb_map_window(m_connection, m_id);
    xcb_aux_sync(m_connection);
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
        KeyPair{"AB06", Key::N}, KeyPair{"AB07", Key::M}, KeyPair{"LFSH", Key::Shift},
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
    if (keycode >= m_keycode_map.size()) {
        return Key::Unknown;
    }
    return m_keycode_map[keycode];
}

Swapchain Window::create_swapchain(const Context &context) {
    vk::XcbSurfaceCreateInfoKHR surface_ci{
        .sType = vk::StructureType::XcbSurfaceCreateInfoKHR,
        .connection = m_connection,
        .window = m_id,
    };
    vk::SurfaceKHR surface;
    context.vkCreateXcbSurfaceKHR(&surface_ci, &surface);
    return {context, {m_width, m_height}, surface};
}

void Window::close() {
    m_should_close = true;
}

void Window::poll_events() {
    m_delta_x = 0;
    m_delta_y = 0;
    xcb_generic_event_t *event;
    while ((event = xcb_poll_for_event(m_connection)) != nullptr) {
        switch (event->response_type & ~0x80u) {
        case XCB_KEY_PRESS: {
            auto *key_press_event = reinterpret_cast<xcb_key_press_event_t *>(event);
            m_keys[static_cast<uint8_t>(translate_keycode(key_press_event->detail))] = true;
            break;
        }
        case XCB_KEY_RELEASE: {
            auto *key_release_event = reinterpret_cast<xcb_key_release_event_t *>(event);
            m_keys[static_cast<uint8_t>(translate_keycode(key_release_event->detail))] = false;
            break;
        }
        case XCB_MOTION_NOTIFY: {
            auto *motion_event = reinterpret_cast<xcb_motion_notify_event_t *>(event);
            m_delta_x = motion_event->event_x - static_cast<int16_t>(m_width / 2);  // NOLINT
            m_delta_y = motion_event->event_y - static_cast<int16_t>(m_height / 2); // NOLINT
            if (motion_event->event_x != m_width / 2 || motion_event->event_y != m_height / 2) {
                xcb_warp_pointer(m_connection, m_id, m_id, 0, 0, m_width, m_height, static_cast<int16_t>(m_width / 2),
                                 static_cast<int16_t>(m_height / 2));
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
        }
        free(event);
    }
    xcb_flush(m_connection);
}

} // namespace vull
