#include <vull/ui/Window.hh>

#include <vull/core/Input.hh>
#include <vull/maths/Colour.hh>
#include <vull/maths/Common.hh>
#include <vull/support/Optional.hh>
#include <vull/support/String.hh>
#include <vull/support/Utility.hh>
#include <vull/ui/Event.hh>
#include <vull/ui/Painter.hh>
#include <vull/ui/Tree.hh>
#include <vull/ui/Units.hh>
#include <vull/ui/layout/BoxLayout.hh>
#include <vull/ui/layout/Pane.hh>
#include <vull/ui/widget/Label.hh>

namespace vull::ui {

Window::Window(Tree &tree, Optional<Element &> parent, String title) : Pane(tree, parent) {
    m_title_pane = &add_child<HBoxLayout>();
    m_title_pane->margins().set_all(Length::make_cm(0.1f));
    m_title_pane->add_child<Label>(vull::move(title));

    m_content_pane = &add_child<VBoxLayout>();
    m_content_pane->margins().set_top(Length::make_cm(0.3f));
    m_content_pane->margins().set_bottom(Length::make_cm(0.3f));
    m_content_pane->margins().set_left(Length::make_cm(0.5f));
    m_content_pane->margins().set_right(Length::make_cm(0.5f));
}

void Window::paint(Painter &painter, LayoutPoint position) const {
    Colour colour = Colour::from_srgb(0.13f, 0.14f, 0.15f);
    if (m_is_resizing) {
        colour = Colour::from_srgb(0.18f, 0.19f, 0.20f);
    }

    // Title pane background.
    painter.draw_rect(position, {computed_width(), m_title_pane->computed_height()}, Colour::black());

    // Content pane background.
    painter.draw_rect(position + m_content_pane->offset_in_parent(),
                      {computed_width(), computed_height() - m_title_pane->computed_height()}, colour);

    // Paint children.
    Pane::paint(painter, position);
}

bool Window::mouse_in_resize_grab(LayoutPoint position) const {
    // TODO: Better resize grab detection.
    const auto delta = LayoutDelta(computed_size() - position);
    return delta.dx() < LayoutUnit::from_int_pixels(15) && delta.dy() < LayoutUnit::from_int_pixels(20);
}

bool Window::handle_mouse_press(const MouseButtonEvent &event) {
    if (event.button() == MouseButton::Left) {
        tree().set_active_element(*this);
        if (mouse_in_resize_grab(event.position())) {
            m_is_resizing = true;
        }
    }
    return true;
}

bool Window::handle_mouse_release(const MouseButtonEvent &event) {
    if (event.button() == MouseButton::Left) {
        tree().unset_active_element();
        m_is_resizing = false;
    }
    return true;
}

void Window::handle_mouse_move(const MouseMoveEvent &event) {
    if (m_is_resizing) {
        // TODO: This jumps if mouse not pefectly at corner.
        set_computed_size({event.position().x(), event.position().y()});
    } else if (is_active_element()) {
        // TODO: Would be better not to use mouse delta.
        set_offset_in_parent(offset_in_parent() + event.delta());
    }
}

void Window::pre_layout(LayoutSize) {
    m_title_pane->pre_layout(computed_size());
    m_content_pane->pre_layout(computed_size());
}

void Window::layout(LayoutSize) {
    auto title_pane_min_size = m_title_pane->minimum_size().resolve(tree(), computed_size());
    auto content_pane_min_size = m_content_pane->minimum_size().resolve(tree(), computed_size());

    set_computed_width(
        vull::max(computed_width(), vull::max(title_pane_min_size.width(), content_pane_min_size.width())));
    set_computed_height(vull::max(computed_height(), title_pane_min_size.height() + content_pane_min_size.height()));

    auto title_pane_height = title_pane_min_size.height();
    m_title_pane->layout({computed_width(), title_pane_height});
    m_content_pane->set_offset_in_parent({0, title_pane_height});
    m_content_pane->layout({computed_width(), computed_height() - title_pane_height});
}

} // namespace vull::ui
