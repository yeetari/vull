#include <vull/core/Window.hh>

#include <vull/support/Assert.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/Swapchain.hh>

#include <stdlib.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_xcb.h>
#include <xcb/xcb_aux.h>
#include <xcb/xproto.h>

namespace vull {

Window::Window(uint32_t width, uint32_t height, bool fullscreen) : m_width(width), m_height(height) {
    // Open an X connection.
    m_connection = xcb_connect(nullptr, nullptr);
    VULL_ENSURE(xcb_connection_has_error(m_connection) == 0, "Failed to create X connection");

    // Create a window on the first screen and set the title.
    m_id = xcb_generate_id(m_connection);
    const uint32_t event_mask = 0;
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

    // Make the window visible and wait for the server to process the requests.
    xcb_map_window(m_connection, m_id);
    xcb_aux_sync(m_connection);
}

Window::~Window() {
    free(m_delete_window_atom);
    xcb_destroy_window(m_connection, m_id);
    xcb_disconnect(m_connection);
}

Swapchain Window::create_swapchain(const Context &context) {
    VkXcbSurfaceCreateInfoKHR surface_ci{
        .sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
        .connection = m_connection,
        .window = m_id,
    };
    VkSurfaceKHR surface = nullptr;
    vkCreateXcbSurfaceKHR(context.instance(), &surface_ci, nullptr, &surface);
    return {context, {m_width, m_height}, surface};
}

void Window::close() {
    m_should_close = true;
}

void Window::poll_events() {
    xcb_generic_event_t *event;
    while ((event = xcb_poll_for_event(m_connection)) != nullptr) {
        switch (event->response_type & ~0x80u) {
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
