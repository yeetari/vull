#include <vull/ui/widget/Button.hh>

#include <vull/maths/Colour.hh>
#include <vull/support/Function.hh>
#include <vull/support/Optional.hh>
#include <vull/support/String.hh>
#include <vull/support/Utility.hh>
#include <vull/ui/Element.hh>
#include <vull/ui/Painter.hh>
#include <vull/ui/Tree.hh>
#include <vull/ui/Units.hh>
#include <vull/ui/widget/Label.hh>

namespace vull::ui {

Button::Button(Tree &tree, Optional<Element &> parent, String text) : Element(tree, parent), m_label(tree, *this) {
    set_text(vull::move(text));
}

void Button::paint(Painter &painter, LayoutPoint position) const {
    auto colour = Colour::from_srgb(0.25f, 0.25f, 0.25f);
    if (is_hovered()) {
        colour = Colour::from_srgb(0.38f, 0.38f, 0.38f);
    }
    if (is_active_element()) {
        colour = Colour::from_srgb(0.67f, 0.67f, 0.67f, 0.39f);
    }

    painter.draw_rect(position, computed_size(), colour);
    m_label.paint(painter, position + computed_size() / 2 - m_label.computed_size() / 2);
}

bool Button::handle_mouse_press(const MouseButtonEvent &) {
    tree().set_active_element(*this);
    return true;
}

bool Button::handle_mouse_release(const MouseButtonEvent &) {
    tree().unset_active_element();
    if (m_on_release) {
        m_on_release();
    }
    return true;
}

void Button::set_text(String text) {
    m_label.set_text(vull::move(text));

    LayoutUnit padding = m_padding.resolve(tree());
    set_minimum_size(m_label.computed_size() + LayoutSize(padding, padding));
    set_maximum_height(minimum_size().height());
}

} // namespace vull::ui
