#include <vull/ui/Window.hh>

#include <vull/container/Vector.hh>
#include <vull/core/Input.hh>
#include <vull/maths/Colour.hh>
#include <vull/maths/Common.hh>
#include <vull/maths/Vec.hh>
#include <vull/support/Optional.hh>
#include <vull/support/String.hh>
#include <vull/support/Utility.hh>
#include <vull/ui/Event.hh>
#include <vull/ui/Painter.hh>
#include <vull/ui/Tree.hh>
#include <vull/ui/layout/BoxLayout.hh>
#include <vull/ui/layout/Pane.hh>
#include <vull/ui/widget/Label.hh>

namespace vull::ui {

Window::Window(Tree &tree, Optional<Element &> parent, String title) : Pane(tree, parent) {
    m_title_pane = &add_child<HBoxLayout>();
    m_title_pane->margins().set_all(0.1f);
    m_title_pane->add_child<Label>(vull::move(title));

    m_content_pane = &add_child<VBoxLayout>();
    m_content_pane->margins().set(0.3f, 0.3f, 0.5f, 0.5f);
}

void Window::paint(Painter &painter, Vec2f position) const {
    // Title pane background.
    painter.draw_rect(position + m_title_pane->offset_in_parent(),
                      {preferred_size().x(), m_title_pane->preferred_size().y()}, Colour::black());

    // Content pane background.
    painter.draw_rect(position + m_content_pane->offset_in_parent(),
                      {preferred_size().x(), m_content_pane->preferred_size().y()},
                      Colour::from_srgb(0.13f, 0.14f, 0.15f));

    // Paint children.
    Pane::paint(painter, position);
}

bool Window::handle_mouse_press(const MouseButtonEvent &event) {
    if (event.button() == MouseButton::Left) {
        tree().set_active_element(*this);
    }
    return true;
}

bool Window::handle_mouse_release(const MouseButtonEvent &event) {
    if (event.button() == MouseButton::Left) {
        tree().unset_active_element();
    }
    return true;
}

bool Window::handle_mouse_move(const MouseMoveEvent &event) {
    if (is_active_element()) {
        // TODO: Don't use mouse delta.
        set_offset_in_parent(offset_in_parent() + event.delta());
    }
    return true;
}

void Window::layout() {
    Pane::layout();
    m_title_pane->set_offset_in_parent({});
    m_content_pane->set_offset_in_parent({0.0f, m_title_pane->preferred_size().y()});

    const auto width = vull::max(m_title_pane->preferred_size().x(), m_content_pane->preferred_size().x());
    const auto height = m_title_pane->preferred_size().y() + m_content_pane->preferred_size().y();
    set_preferred_size({width, height});
}

} // namespace vull::ui
