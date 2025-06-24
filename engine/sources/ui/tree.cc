#include <vull/ui/tree.hh>

#include <vull/core/input.hh>
#include <vull/maths/vec.hh>
#include <vull/support/assert.hh>
#include <vull/support/optional.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>
#include <vull/ui/element.hh>
#include <vull/ui/event.hh>
#include <vull/ui/layout/pane.hh>
#include <vull/ui/units.hh>

namespace vull::ui {

void Tree::render(Painter &painter) {
    if (m_root_element->is_pane()) {
        static_cast<Pane &>(*m_root_element).pre_layout({});
        static_cast<Pane &>(*m_root_element).layout({});
    }
    if (vull::exchange(m_need_hover_update, false)) {
        update_hover();
    }
    m_root_element->paint(painter, m_root_element->offset_in_parent());
}

void Tree::update_hover() {
    auto hit_result = m_root_element->hit_test(m_mouse_position - m_root_element->offset_in_parent());
    if (!hit_result) {
        // Nothing hovered.
        if (m_hovered_element) {
            MouseEvent exit_event(m_hovered_relative_position, m_mouse_buttons);
            m_hovered_element->handle_mouse_exit(exit_event);
        }
        m_hovered_element = {};
        return;
    }

    // Update mouse position relative to the currently hovered element.
    m_hovered_relative_position = hit_result->relative_position;

    auto &hovered_element = hit_result->element;
    if (&hovered_element == m_hovered_element) {
        // Same element hovered.
        return;
    }

    MouseEvent enter_exit_event(m_hovered_relative_position, m_mouse_buttons);
    if (m_hovered_element) {
        m_hovered_element->handle_mouse_exit(enter_exit_event);
    }
    m_hovered_element = hovered_element;
    if (!m_active_element) {
        // Only send mouse enter if an element isn't hijacking input events. This prevents a button from being
        // highlighted from hovering when dragging a slider, for example.
        m_hovered_element->handle_mouse_enter(enter_exit_event);
    }
}

void Tree::handle_element_destruct(Element &element) {
    if (m_active_element == &element) {
        unset_active_element();
    }
    if (m_hovered_element == &element) {
        unset_hovered_element();
    }
}

void Tree::handle_element_hide(Element &element) {
    // Unset the active and/or hovered elements if their parents have been hidden.
    for (auto parent = m_active_element; parent; parent = parent->parent()) {
        if (parent == &element) {
            unset_active_element();
            break;
        }
    }
    for (auto parent = m_hovered_element; parent; parent = parent->parent()) {
        if (parent == &element) {
            unset_hovered_element();
            break;
        }
    }
}

void Tree::handle_element_show(Element &) {
    // Dirty the current hover in case something new has become visible under the cursor.
    m_need_hover_update = true;
}

void Tree::set_active_element(Element &element) {
    VULL_ASSERT(!m_active_element);
    m_active_element = element;
}

void Tree::unset_active_element() {
    m_active_element = {};
    m_need_hover_update = true;
}

void Tree::unset_hovered_element() {
    m_hovered_element = {};
    m_need_hover_update = true;
}

// Helper for calculating a position relative to an element given a global screen position. For example, there may be an
// active element hijacking input events. This means that the mouse may be outside the active element but still
// interacting with it (e.g. moving a slider). We still need to calculate a position for the mouse relative to the
// active element to pass to the event handler.
static LayoutPoint calculate_element_relative_position(Optional<Element &> element, LayoutPoint global_position) {
    LayoutPoint relative_position = global_position;
    for (; element; element = element->parent()) {
        relative_position -= element->offset_in_parent();
    }
    return relative_position;
}

template <auto event_fn>
static void dispatch_event(Optional<Element &> element, const auto &event) {
    for (; element; element = element->parent()) {
        if ((*element.*event_fn)(event)) {
            return;
        }
    }
}

template <auto event_fn>
void Tree::handle_mouse_press_release(MouseButton button) {
    if (!m_active_element) {
        // No active element hijacking events, just send the event to the hovered element if present. Note that we need
        // to re-relativise the mouse position, so we can't use dispatch_event.
        // TODO: Find a way to use dispatch_event?
        auto relative_position = m_hovered_relative_position;
        for (auto element = m_hovered_element; element; element = element->parent()) {
            MouseButtonEvent event(relative_position, m_mouse_buttons, button);
            if ((*element.*event_fn)(event)) {
                return;
            }
            relative_position += element->offset_in_parent();
        }
        return;
    }

    // Otherwise there is an active element hijacking input events.
    const auto relative_position = calculate_element_relative_position(m_active_element, m_mouse_position);
    MouseButtonEvent event(relative_position, m_mouse_buttons, button);
    dispatch_event<event_fn>(m_active_element, event);
}

void Tree::handle_mouse_press(MouseButton button) {
    handle_mouse_press_release<&Element::handle_mouse_press>(button);
}

void Tree::handle_mouse_release(MouseButton button) {
    handle_mouse_press_release<&Element::handle_mouse_release>(button);
}

void Tree::handle_mouse_move(Vec2i delta, Vec2u position, MouseButtonMask buttons) {
    m_mouse_position = LayoutPoint::from_int_pixels(position);
    m_mouse_buttons = buttons;

    // Update the currently hovered element.
    update_hover();

    // TODO: Should mouse move events propagate? (use dispatch_event)
    const auto layout_delta = LayoutDelta::from_int_pixels(delta);
    if (!m_active_element) {
        // No active element hijacking events, just send the move to the hovered element if present.
        if (m_hovered_element) {
            MouseMoveEvent move_event(m_hovered_relative_position, m_mouse_buttons, layout_delta);
            m_hovered_element->handle_mouse_move(move_event);
        }
        return;
    }

    // Otherwise there is an active element hijacking move events.
    const auto relative_position = calculate_element_relative_position(m_active_element, m_mouse_position);
    MouseMoveEvent move_event(relative_position, buttons, layout_delta);
    m_active_element->handle_mouse_move(move_event);
}

} // namespace vull::ui
