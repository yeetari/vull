#include <vull/core/input.hh>
#include <vull/core/log.hh>
#include <vull/maths/epsilon.hh>
#include <vull/platform/file.hh>
#include <vull/platform/window.hh>
#include <vull/platform/xkb.hh>
#include <vull/support/span.hh>
#include <vull/support/string_view.hh>
#include <vull/support/utility.hh>
#include <vull/vulkan/vulkan.hh>

#include <linux/input-event-codes.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>
#include <wayland-cursor.h>
#include <wayland-egl.h>
#include <wayland-pointer-constraints-client-protocol.h>
#include <wayland-relative-pointer-client-protocol.h>
#include <wayland-util.h>
#include <wayland-xdg-decoration-client-protocol.h>
#include <wayland-xdg-shell-client-protocol.h>
#include <xkbcommon/xkbcommon-compat.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon.h>

#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>

namespace vull::platform {
namespace {

template <typename... Ts>
void noop(Ts...) {}

class WaylandGlobals {
    wl_display *m_display;
    wl_registry *m_registry;

    // registry globals
    wl_compositor *m_compositor{nullptr};
    wl_seat *m_seat{nullptr};
    wl_shm *m_shm{nullptr};
    wl_output *m_output{nullptr};
    xdg_wm_base *m_wm_base{nullptr};
    zxdg_decoration_manager_v1 *m_decoration_manager{nullptr};
    zwp_pointer_constraints_v1 *m_pointer_constraints{nullptr};
    zwp_relative_pointer_manager_v1 *m_relative_pointer_manager{nullptr};
    zwp_relative_pointer_v1 *m_relative_pointer{nullptr};

    // seat
    wl_pointer *m_pointer{nullptr};
    wl_keyboard *m_keyboard{nullptr};

    // PPCM calculation.
    int32_t m_output_height{-1};
    int32_t m_output_height_mm{-1};
    float m_ppcm{38.0f};

public:
    WaylandGlobals(wl_display *display, wl_registry *registry) : m_display(display), m_registry(registry) {}
    WaylandGlobals(WaylandGlobals &) = delete;
    WaylandGlobals(WaylandGlobals &&o) {
        m_display = vull::exchange(o.m_display, nullptr);
        m_registry = vull::exchange(o.m_registry, nullptr);

        m_compositor = vull::exchange(o.m_compositor, nullptr);
        m_seat = vull::exchange(o.m_seat, nullptr);
        m_shm = vull::exchange(o.m_shm, nullptr);
        m_output = vull::exchange(o.m_output, nullptr);
        m_wm_base = vull::exchange(o.m_wm_base, nullptr);
        m_decoration_manager = vull::exchange(o.m_decoration_manager, nullptr);
        m_pointer_constraints = vull::exchange(o.m_pointer_constraints, nullptr);
        m_relative_pointer_manager = vull::exchange(o.m_relative_pointer_manager, nullptr);
        m_relative_pointer = vull::exchange(o.m_relative_pointer, nullptr);

        m_pointer = vull::exchange(o.m_pointer, nullptr);
        m_keyboard = vull::exchange(o.m_keyboard, nullptr);
    }
    ~WaylandGlobals() {
        if (m_relative_pointer != nullptr) {
            zwp_relative_pointer_v1_destroy(m_relative_pointer);
        }
        if (m_keyboard != nullptr) {
            wl_keyboard_release(m_keyboard);
        }
        if (m_pointer != nullptr) {
            wl_pointer_release(m_pointer);
        }

        if (m_decoration_manager != nullptr) {
            zxdg_decoration_manager_v1_destroy(m_decoration_manager);
        }
        if (m_pointer_constraints != nullptr) {
            zwp_pointer_constraints_v1_destroy(m_pointer_constraints);
        }
        if (m_relative_pointer_manager != nullptr) {
            zwp_relative_pointer_manager_v1_destroy(m_relative_pointer_manager);
        }
        if (m_wm_base != nullptr) {
            xdg_wm_base_destroy(m_wm_base);
        }
        if (m_output != nullptr) {
            wl_output_destroy(m_output);
        }
        if (m_shm != nullptr) {
            wl_shm_destroy(m_shm);
        }
        if (m_seat != nullptr) {
            wl_seat_destroy(m_seat);
        }
        if (m_compositor != nullptr) {
            wl_compositor_destroy(m_compositor);
        }
        if (m_registry != nullptr) {
            wl_registry_destroy(m_registry);
        }

        if (m_display != nullptr) {
            wl_display_disconnect(m_display);
        }
    }

    WaylandGlobals &operator=(const WaylandGlobals &) = delete;
    WaylandGlobals &operator=(WaylandGlobals &&) = delete;

    bool has_required() const {
        // m_decoration_manager and m_pointer_constraints are optional.
        return m_compositor != nullptr && m_seat != nullptr && m_shm != nullptr && m_wm_base != nullptr &&
               m_relative_pointer_manager != nullptr;
    }

    wl_display *display() const { return m_display; }

    wl_compositor *compositor() const { return m_compositor; }
    wl_shm *shm() const { return m_shm; }
    xdg_wm_base *wm_base() const { return m_wm_base; }
    zxdg_decoration_manager_v1 *decoration_manager() const { return m_decoration_manager; }
    zwp_pointer_constraints_v1 *pointer_constraints() const { return m_pointer_constraints; }

    wl_pointer *pointer() const { return m_pointer; }
    wl_keyboard *keyboard() const { return m_keyboard; }
    zwp_relative_pointer_v1 *relative_pointer() const { return m_relative_pointer; }

    float ppcm() const { return m_ppcm; }

    static void on_registry_global(void *globals_ptr, wl_registry *registry, uint32_t id, const char *interface_ptr,
                                   uint32_t /* name */) noexcept {
        auto &globals = *static_cast<WaylandGlobals *>(globals_ptr);
        StringView interface = interface_ptr;

        if (interface == wl_compositor_interface.name) {
            globals.m_compositor =
                static_cast<wl_compositor *>(wl_registry_bind(registry, id, &wl_compositor_interface, 1));
        } else if (interface == xdg_wm_base_interface.name) {
            globals.m_wm_base = static_cast<xdg_wm_base *>(wl_registry_bind(registry, id, &xdg_wm_base_interface, 1));
        } else if (interface == wl_shm_interface.name) {
            globals.m_shm = static_cast<wl_shm *>(wl_registry_bind(registry, id, &wl_shm_interface, 2));
        } else if (interface == wl_seat_interface.name) {
            globals.m_seat = static_cast<wl_seat *>(wl_registry_bind(registry, id, &wl_seat_interface, 7));
            wl_seat_add_listener(globals.m_seat, &k_seat_listener, globals_ptr);
        } else if (interface == wl_output_interface.name) {
            globals.m_output = static_cast<wl_output *>(wl_registry_bind(registry, id, &wl_output_interface, 4));
            wl_output_add_listener(globals.m_output, &k_output_listener, globals_ptr);
        } else if (interface == zxdg_decoration_manager_v1_interface.name) {
            globals.m_decoration_manager = static_cast<zxdg_decoration_manager_v1 *>(
                wl_registry_bind(registry, id, &zxdg_decoration_manager_v1_interface, 1));
        } else if (interface == zwp_pointer_constraints_v1_interface.name) {
            globals.m_pointer_constraints = static_cast<zwp_pointer_constraints_v1 *>(
                wl_registry_bind(registry, id, &zwp_pointer_constraints_v1_interface, 1));
        } else if (interface == zwp_relative_pointer_manager_v1_interface.name) {
            globals.m_relative_pointer_manager = static_cast<zwp_relative_pointer_manager_v1 *>(
                wl_registry_bind(registry, id, &zwp_relative_pointer_manager_v1_interface, 1));
        }
    }
    static void on_registry_global_remove(void * /* globals_ptr */, wl_registry * /* registry */,
                                          uint32_t /* name */) noexcept {}
    static constexpr wl_registry_listener k_registry_listener{on_registry_global, on_registry_global_remove};

    static void on_seat_capabilities(void *globals_ptr, wl_seat *seat, uint32_t capabilities) noexcept {
        auto &globals = *static_cast<WaylandGlobals *>(globals_ptr);

        const bool had_keyboard = globals.m_keyboard != nullptr;
        const bool has_keyboard = (capabilities & WL_SEAT_CAPABILITY_KEYBOARD) != 0;
        if (has_keyboard && !had_keyboard) {
            globals.m_keyboard = wl_seat_get_keyboard(seat);
        } else if (!has_keyboard && had_keyboard) {
            wl_keyboard_release(vull::exchange(globals.m_keyboard, nullptr));
        }

        const bool had_pointer = globals.m_pointer != nullptr;
        const bool has_pointer = (capabilities & WL_SEAT_CAPABILITY_POINTER) != 0;
        if (has_pointer && !had_pointer) {
            globals.m_pointer = wl_seat_get_pointer(seat);
            globals.m_relative_pointer = zwp_relative_pointer_manager_v1_get_relative_pointer(
                globals.m_relative_pointer_manager, globals.m_pointer);
        } else if (!has_pointer && had_pointer) {
            zwp_relative_pointer_v1_destroy(vull::exchange(globals.m_relative_pointer, nullptr));
            wl_pointer_release(vull::exchange(globals.m_pointer, nullptr));
        }
    }
    static void on_seat_name(void * /* globals_ptr */, wl_seat * /* seat */, const char * /* name */) noexcept {}
    static constexpr wl_seat_listener k_seat_listener{on_seat_capabilities, on_seat_name};

    static void on_output_geometry(void *globals_ptr, wl_output * /* output */, int32_t /* x */, int32_t /* y */,
                                   int32_t physical_width, int32_t physical_height, int32_t /* subpixel */,
                                   const char * /* make */, const char *model_c, int32_t /* transform */) {
        auto &globals = *static_cast<WaylandGlobals *>(globals_ptr);

        // Monitor seems to have broken EDID data.
        StringView model(model_c);
        if (model == "AG241QG4") {
            physical_height = 298;
        }
        globals.m_output_height_mm = physical_height;
        vull::debug("[wayland] Output geometry is {} mm x {} mm ({})", physical_width, physical_height, model);
    }
    static void on_output_mode(void *globals_ptr, wl_output * /* output */, uint32_t flags, int32_t width,
                               int32_t height, int32_t /* refresh */) {
        if ((flags & WL_OUTPUT_MODE_CURRENT) == 0) {
            return;
        }
        auto &globals = *static_cast<WaylandGlobals *>(globals_ptr);
        globals.m_output_height = height;
        vull::debug("[wayland] Output resolution is {} x {}", width, height);
    }
    static void on_output_done(void *globals_ptr, wl_output * /* output */
    ) {
        auto &globals = *static_cast<WaylandGlobals *>(globals_ptr);
        if (globals.m_output_height <= 0 || globals.m_output_height_mm <= 0) {
            return;
        }

        const float height_cm = static_cast<float>(globals.m_output_height_mm) / 10.0f;
        globals.m_ppcm = static_cast<float>(globals.m_output_height) / height_cm;
        vull::debug("[wayland] Output ppcm is {}", globals.m_ppcm);
    }
    static constexpr wl_output_listener k_output_listener{
        .geometry = on_output_geometry,
        .mode = on_output_mode,
        .done = on_output_done,
        .scale = noop,
        .name = noop,
        .description = noop,
    };
};

class WindowWayland : public Window {
    WaylandGlobals m_globals;
    wl_surface *m_window_surface;
    xdg_surface *m_xdg_surface;
    xdg_toplevel *m_xdg_toplevel;
    wl_region *m_window_region;
    zxdg_toplevel_decoration_v1 *m_toplevel_decoration;
    xkb_context *m_xkb_context{nullptr};
    xkb_state *m_xkb_state{nullptr};
    zwp_locked_pointer_v1 *m_locked_pointer{nullptr};
    uint32_t m_pointer_enter_serial{0};
    Vec2u m_desired_resolution;

    // cursor
    wl_surface *m_left_ptr;

public:
    WindowWayland(WaylandGlobals &&globals, wl_surface *window_surface, xdg_surface *xdg_surface,
                  xdg_toplevel *xdg_toplevel, wl_region *window_region,
                  zxdg_toplevel_decoration_v1 *toplevel_decoration, xkb_context *xkb_context, wl_surface *left_ptr,
                  Vec2u desired_resolution)
        : Window(globals.ppcm()), m_globals(vull::move(globals)), m_window_surface(window_surface),
          m_xdg_surface(xdg_surface), m_xdg_toplevel(xdg_toplevel), m_window_region(window_region),
          m_toplevel_decoration(toplevel_decoration), m_xkb_context(xkb_context),
          m_desired_resolution(desired_resolution), m_left_ptr(left_ptr) {
        xdg_wm_base_add_listener(m_globals.wm_base(), &k_wm_base_listener, this);
        xdg_surface_add_listener(m_xdg_surface, &k_xdg_surface_listener, this);
        xdg_toplevel_add_listener(m_xdg_toplevel, &k_xdg_toplevel_listener, this);

        // TODO: Need to re-add listener if keyboard/pointer is removed and re-added.
        wl_keyboard_add_listener(m_globals.keyboard(), &k_keyboard_listener, this);
        wl_pointer_add_listener(m_globals.pointer(), &k_pointer_listener, this);
        zwp_relative_pointer_v1_add_listener(m_globals.relative_pointer(), &k_relative_pointer_listener, this);
    }
    WindowWayland(const WindowWayland &) = delete;
    WindowWayland(WindowWayland &&) = delete;
    ~WindowWayland() override {
        xkb_state_unref(m_xkb_state);
        xkb_context_unref(m_xkb_context);

        if (m_locked_pointer != nullptr) {
            zwp_locked_pointer_v1_destroy(m_locked_pointer);
        }
        if (m_toplevel_decoration != nullptr) {
            zxdg_toplevel_decoration_v1_destroy(m_toplevel_decoration);
        }
        wl_region_destroy(m_window_region);
        xdg_toplevel_destroy(m_xdg_toplevel);
        xdg_surface_destroy(m_xdg_surface);
        wl_surface_destroy(m_window_surface);

        wl_surface_destroy(m_left_ptr);
    }

    WindowWayland &operator=(const WindowWayland &) = delete;
    WindowWayland &operator=(WindowWayland &&) = delete;

    Result<vk::Swapchain, vkb::Result> create_swapchain(vk::Context &context, vk::SwapchainMode mode) override {
        vkb::WaylandSurfaceCreateInfoKHR surface_ci{
            .sType = vkb::StructureType::WaylandSurfaceCreateInfoKHR,
            .display = m_globals.display(),
            .surface = m_window_surface,
        };
        vkb::SurfaceKHR surface;
        if (auto result = context.vkCreateWaylandSurfaceKHR(&surface_ci, &surface); result != vkb::Result::Success) {
            return result;
        }
        return vk::Swapchain{context, surface, mode};
    }
    void poll_events() override { wl_display_dispatch_pending(m_globals.display()); }
    Span<const char *const> required_extensions() const override {
        static constexpr Array extensions{
            "VK_KHR_surface",
            "VK_KHR_wayland_surface",
        };
        return extensions.span();
    }

    void grab_cursor() override {
        if (m_globals.pointer_constraints() != nullptr && m_locked_pointer == nullptr) {
            m_cursor_grabbed = true;
            m_locked_pointer = zwp_pointer_constraints_v1_lock_pointer(m_globals.pointer_constraints(),
                                                                       m_window_surface, m_globals.pointer(), nullptr,
                                                                       ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_ONESHOT);

            // The serial parameter must match the latest wl_pointer.enter serial number
            // sent to the client. Otherwise the request will be ignored.
            wl_pointer_set_cursor(m_globals.pointer(), m_pointer_enter_serial, nullptr, 0, 0);
        }
    }
    void ungrab_cursor() override {
        if (m_locked_pointer != nullptr) {
            m_cursor_grabbed = false;
            zwp_locked_pointer_v1_destroy(vull::exchange(m_locked_pointer, nullptr));

            wl_pointer_set_cursor(m_globals.pointer(), m_pointer_enter_serial, m_left_ptr, 0, 0);
        }
    }
    void set_fullscreen(bool fullscreen) override {
        if (fullscreen) {
            xdg_toplevel_set_fullscreen(m_xdg_toplevel, nullptr);
        } else {
            xdg_toplevel_unset_fullscreen(m_xdg_toplevel);
        }
        m_is_fullscreen = fullscreen;
    }

    void handle_key(xkb_keysym_t sym, bool pressed) {
        auto mask = static_cast<ModifierMask>(0);
        xkb_mod_index_t alt = xkb_keymap_mod_get_index(xkb_state_get_keymap(m_xkb_state), XKB_MOD_NAME_ALT);
        if (xkb_state_mod_index_is_active(m_xkb_state, alt, XKB_STATE_MODS_DEPRESSED) == 1) {
            mask |= ModifierMask::Alt;
        }
        const Key key = xkb_translate_key(xkb_keysym_to_lower(sym));
        m_keys[vull::to_underlying(key)] = pressed;
        if (pressed && m_key_press_callbacks.contains(key)) {
            if (auto &callback = *m_key_press_callbacks.get(key)) {
                callback(static_cast<ModifierMask>(mask));
            }
        }
        if (!pressed && m_key_release_callbacks.contains(key)) {
            if (auto &callback = *m_key_release_callbacks.get(key)) {
                callback(static_cast<ModifierMask>(mask));
            }
        }
    }

    static void on_wm_base_ping(void * /* globals_ptr */, xdg_wm_base *wm_base, uint32_t serial) noexcept {
        xdg_wm_base_pong(wm_base, serial);
    }
    static constexpr xdg_wm_base_listener k_wm_base_listener{.ping = on_wm_base_ping};

    static void on_xdg_surface_configure(void *window_ptr, xdg_surface *xdg_surface, uint32_t serial) noexcept {
        auto &window = *static_cast<WindowWayland *>(window_ptr);
        window.m_resolution = window.m_desired_resolution;
        xdg_surface_ack_configure(xdg_surface, serial);
    }
    static constexpr xdg_surface_listener k_xdg_surface_listener{.configure = on_xdg_surface_configure};

    static void on_xdg_toplevel_configure(void *window_ptr, xdg_toplevel *, int32_t width, int32_t height,
                                          wl_array * /* no idea */) noexcept {
        if (width <= 0 || height <= 0) {
            return;
        }
        auto &window = *static_cast<WindowWayland *>(window_ptr);
        window.m_desired_resolution.set_x(static_cast<uint32_t>(width));
        window.m_desired_resolution.set_y(static_cast<uint32_t>(height));
    }
    static void on_xdg_toplevel_close(void *window_ptr, xdg_toplevel *) noexcept {
        auto &window = *static_cast<WindowWayland *>(window_ptr);
        window.m_close_callback();
    }
    static constexpr xdg_toplevel_listener k_xdg_toplevel_listener{.configure = on_xdg_toplevel_configure,
                                                                   .close = on_xdg_toplevel_close};

    static void on_keyboard_keymap(void *window_ptr, wl_keyboard *, uint32_t /* format */, int32_t fd,
                                   uint32_t size) noexcept {
        auto &window = *static_cast<WindowWayland *>(window_ptr);
        platform::File file = platform::File::from_fd(fd);

        // TODO(file): map
        void *shm = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, file.fd(), 0);
        xkb_keymap *xkb_keymap = xkb_keymap_new_from_string(window.m_xkb_context, static_cast<const char *>(shm),
                                                            XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
        munmap(shm, size);

        xkb_state *xkb_state = xkb_state_new(xkb_keymap);
        xkb_keymap_unref(xkb_keymap);
        xkb_state_unref(vull::exchange(window.m_xkb_state, xkb_state));
    }
    static void on_keyboard_enter(void *window_ptr, wl_keyboard *, uint32_t /* serial */, wl_surface * /* surface */,
                                  wl_array *keys_array) noexcept {
        auto &window = *static_cast<WindowWayland *>(window_ptr);

        const Span<uint32_t> keys(static_cast<uint32_t *>(keys_array->data), keys_array->size / sizeof(uint32_t));
        for (auto key : keys) {
            // Add 8 to convert from an evdev scancode to an xkb scancode.
            const xkb_keysym_t sym = xkb_state_key_get_one_sym(window.m_xkb_state, key + 8);
            window.handle_key(sym, true);
        }
    }
    static void on_keyboard_key(void *window_ptr, wl_keyboard *, uint32_t, uint32_t, uint32_t key,
                                uint32_t state) noexcept {
        auto &window = *static_cast<WindowWayland *>(window_ptr);
        // Add 8 to convert from an evdev scancode to an xkb scancode.
        const xkb_keysym_t sym = xkb_state_key_get_one_sym(window.m_xkb_state, key + 8);
        window.handle_key(sym, state == WL_KEYBOARD_KEY_STATE_PRESSED);
    }
    static void on_keyboard_modifiers(void *window_ptr, wl_keyboard *, uint32_t /* serial */, uint32_t mods_depressed,
                                      uint32_t mods_latched, uint32_t mods_locked, uint32_t group) noexcept {
        auto &window = *static_cast<WindowWayland *>(window_ptr);
        xkb_state_update_mask(window.m_xkb_state, mods_depressed, mods_latched, mods_locked, 0, 0, group);
    }
    static constexpr wl_keyboard_listener k_keyboard_listener{
        .keymap = on_keyboard_keymap,
        .enter = on_keyboard_enter,
        .leave = noop,
        .key = on_keyboard_key,
        .modifiers = on_keyboard_modifiers,
        .repeat_info = noop,
    };

    static void on_pointer_enter(void *window_ptr, wl_pointer * /* pointer */, uint32_t serial,
                                 wl_surface * /* surface */, wl_fixed_t /* hotspot x */,
                                 wl_fixed_t /* hotspot y */) noexcept {
        auto &window = *static_cast<WindowWayland *>(window_ptr);
        window.m_pointer_enter_serial = serial;
        if (window.m_cursor_grabbed) {
            wl_pointer_set_cursor(window.m_globals.pointer(), window.m_pointer_enter_serial, nullptr, 0, 0);
        }
    }
    static void on_pointer_motion(void *window_ptr, wl_pointer * /* pointer */, uint32_t /* serial */,
                                  wl_fixed_t surface_x, wl_fixed_t surface_y) noexcept {
        auto &window = *static_cast<WindowWayland *>(window_ptr);
        if (window.m_cursor_grabbed) {
            // Pointer is locked, rely on relative movement.
            return;
        }
        const auto mouse_x = static_cast<int16_t>(wl_fixed_to_int(surface_x));
        const auto mouse_y = static_cast<int16_t>(wl_fixed_to_int(surface_y));
        const auto delta_x = mouse_x - window.m_mouse_x;
        const auto delta_y = mouse_y - window.m_mouse_y;
        window.m_mouse_x = mouse_x;
        window.m_mouse_y = mouse_y;
        if (window.m_mouse_move_callback) {
            window.m_mouse_move_callback({delta_x, delta_y}, {mouse_x, mouse_y}, window.m_buttons);
        }
    }
    static void on_pointer_button(void *window_ptr, wl_pointer *, uint32_t /* serial */, uint32_t /* time */,
                                  uint32_t button_code, uint32_t state) noexcept {
        auto &window = *static_cast<WindowWayland *>(window_ptr);
        MouseButtonMask button = [](uint32_t button_code) {
            switch (button_code) {
            case BTN_LEFT:
                return MouseButtonMask::Left;
            case BTN_MIDDLE:
                return MouseButtonMask::Middle;
            case BTN_RIGHT:
                return MouseButtonMask::Right;
            default:
                return MouseButtonMask::None;
            }
        }(button_code);

        // TODO: Accumulate buttons held with bitwise OR.
        if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
            window.m_buttons |= button;
            if (auto callback = window.m_mouse_press_callbacks.get(button)) {
                // TODO:
                (*callback)({0, 0});
            }
        }
        if (state == WL_POINTER_BUTTON_STATE_RELEASED) {
            window.m_buttons &= ~button;
            if (auto callback = window.m_mouse_release_callbacks.get(button)) {
                // TODO:
                (*callback)({0, 0});
            }
        }
    }
    static constexpr wl_pointer_listener k_pointer_listener{
        .enter = on_pointer_enter,
        .leave = noop,
        .motion = on_pointer_motion,
        .button = on_pointer_button,
        .axis = noop,
        .frame = noop,
        .axis_source = noop,
        .axis_stop = noop,
        .axis_discrete = noop,
        .axis_value120 = noop,
        .axis_relative_direction = noop,
    };

    static void on_relative_motion(void *window_ptr, zwp_relative_pointer_v1 * /* relative_pointer */,
                                   uint32_t /* utime_hi */, uint32_t /* utime_lo */, wl_fixed_t /* dx */,
                                   wl_fixed_t /* dy */, wl_fixed_t dx_unaccel, wl_fixed_t dy_unaccel) {
        auto &window = *static_cast<WindowWayland *>(window_ptr);
        if (!window.m_cursor_grabbed) {
            // Pointer is unlocked, rely on other motion events.
            return;
        }
        Vec2f delta(dx_unaccel, dy_unaccel);
        delta /= 256.0f;
        if (!vull::fuzzy_zero(delta) && window.m_mouse_move_callback) {
            window.m_mouse_move_callback(delta, {}, window.m_buttons);
        }
    }
    static constexpr zwp_relative_pointer_v1_listener k_relative_pointer_listener{
        .relative_motion = on_relative_motion,
    };
};

} // namespace

Result<UniquePtr<Window>, WindowError> Window::create_wayland(Optional<uint16_t> width, Optional<uint16_t> height) {
    wl_display *display = wl_display_connect(nullptr);
    if (display == nullptr) {
        vull::error("[wayland] Failed to connect to Wayland display");
        return WindowError::ConnectionFailed;
    }

    wl_registry *registry = wl_display_get_registry(display);
    WaylandGlobals globals(display, registry);
    wl_registry_add_listener(registry, &WaylandGlobals::k_registry_listener, &globals);
    wl_display_dispatch(display);
    wl_display_roundtrip(display);
    if (!globals.has_required()) {
        vull::error("[wayland] Failed to get Wayland protocols");
        return WindowError::WaylandMissingProtocol;
    }

    wl_cursor_theme *cursor_theme = wl_cursor_theme_load(nullptr, 32, globals.shm());
    if (cursor_theme == nullptr) {
        vull::error("[wayland] Failed to load cursor theme");
        return WindowError::WaylandError;
    }
    wl_cursor *cursor = wl_cursor_theme_get_cursor(cursor_theme, "left_ptr");
    if (cursor == nullptr || cursor->image_count == 0) {
        vull::error("[wayland] Failed to load left pointer cursor");
        wl_cursor_theme_destroy(cursor_theme);
        return WindowError::WaylandError;
    }
    wl_buffer *cursor_buffer = wl_cursor_image_get_buffer(cursor->images[0]);
    wl_surface *left_ptr_surface = wl_compositor_create_surface(globals.compositor());
    wl_surface_attach(left_ptr_surface, cursor_buffer, 0, 0);
    wl_surface_commit(left_ptr_surface);
    wl_cursor_theme_destroy(cursor_theme);

    wl_surface *window_surface = wl_compositor_create_surface(globals.compositor());
    if (window_surface == nullptr) {
        vull::error("[wayland] Failed to create window surface");
        return WindowError::WaylandError;
    }
    xdg_surface *xdg_surface = xdg_wm_base_get_xdg_surface(globals.wm_base(), window_surface);
    if (xdg_surface == nullptr) {
        vull::error("[wayland] Failed to create xdg surface");
        return WindowError::WaylandError;
    }
    xdg_toplevel *xdg_toplevel = xdg_surface_get_toplevel(xdg_surface);
    if (xdg_toplevel == nullptr) {
        vull::error("[wayland] Failed to create xdg toplevel");
        return WindowError::WaylandError;
    }
    xdg_toplevel_set_title(xdg_toplevel, "Vull");

    // Enable server side decoration, if available.
    zxdg_toplevel_decoration_v1 *toplevel_decoration{nullptr};
    if (globals.decoration_manager() != nullptr) {
        toplevel_decoration =
            zxdg_decoration_manager_v1_get_toplevel_decoration(globals.decoration_manager(), xdg_toplevel);
        zxdg_toplevel_decoration_v1_set_mode(toplevel_decoration, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
    }

    wl_surface_commit(window_surface);

    Vec2u resolution(width.value_or(1280), height.value_or(720));

    wl_region *window_region = wl_compositor_create_region(globals.compositor());
    if (window_region == nullptr) {
        vull::error("[wayland] Failed to create compositor region");
        return WindowError::WaylandError;
    }
    wl_region_add(window_region, 0, 0, static_cast<int32_t>(resolution.x()), static_cast<int32_t>(resolution.y()));
    wl_surface_set_opaque_region(window_surface, window_region);

    xkb_context *xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (xkb_context == nullptr) {
        vull::error("[wayland] failed to create xkb context");
        return WindowError::XkbError;
    }

    return vull::make_unique<WindowWayland>(vull::move(globals), window_surface, xdg_surface, xdg_toplevel,
                                            window_region, toplevel_decoration, xkb_context, left_ptr_surface,
                                            resolution);
}
} // namespace vull::platform
