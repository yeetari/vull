#include <vull/ui/widget/Button.hh>

#include <vull/maths/Colour.hh>
#include <vull/maths/Vec.hh>
#include <vull/support/Function.hh>
#include <vull/support/Optional.hh>
#include <vull/support/String.hh>
#include <vull/support/Utility.hh>
#include <vull/ui/Element.hh>
#include <vull/ui/Painter.hh>
#include <vull/ui/Tree.hh>
#include <vull/ui/widget/Label.hh>

namespace vull::ui {

Button::Button(Tree &tree, Optional<Element &> parent, String text) : Element(tree, parent), m_label(tree, *this) {
    set_text(vull::move(text));
}

void Button::paint(Painter &painter, Vec2f position) const {
    auto colour = Colour::from_srgb(0.25f, 0.25f, 0.25f);
    if (is_hovered()) {
        colour = Colour::from_srgb(0.38f, 0.38f, 0.38f);
    }
    if (is_active_element()) {
        colour = Colour::from_srgb(0.67f, 0.67f, 0.67f, 0.39f);
    }

    painter.draw_rect(position, preferred_size(), colour);
    m_label.paint(painter, position + Vec2f(m_padding * 0.5f));
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
    set_preferred_size(m_label.preferred_size() + Vec2f(m_padding));
}

} // namespace vull::ui
