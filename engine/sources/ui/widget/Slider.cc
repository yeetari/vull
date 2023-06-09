#include <vull/ui/widget/Slider.hh>

#include <vull/core/Input.hh>
#include <vull/maths/Colour.hh>
#include <vull/maths/Common.hh>
#include <vull/maths/Vec.hh>
#include <vull/support/Format.hh>
#include <vull/support/Optional.hh>
#include <vull/ui/Element.hh>
#include <vull/ui/Event.hh>
#include <vull/ui/Painter.hh>
#include <vull/ui/Tree.hh>
#include <vull/ui/widget/Label.hh>

namespace vull::ui {

Slider::Slider(Tree &tree, Optional<Element &> parent, Font &font, float min, float max)
    : Element(tree, parent), m_min(min), m_max(max), m_value_label(tree, *this, font) {
    set_preferred_size({6.0f, 0.5f});
    set_value(min);
}

void Slider::paint(Painter &painter, Vec2f position) const {
    auto colour = Colour::from_srgb(0.25f, 0.25f, 0.25f);
    if (is_hovered()) {
        colour = Colour::from_srgb(0.38f, 0.38f, 0.38f);
    }
    if (is_active_element()) {
        colour = Colour::from_srgb(0.67f, 0.67f, 0.67f, 0.39f);
    }

    // Draw groove.
    painter.draw_rect(position, preferred_size(), colour);

    // Draw handle.
    const float value_ratio = (m_value - m_min) / (m_max - m_min);
    const float handle_x = (preferred_size().x() - m_handle_width - m_handle_padding * 2.0f) * value_ratio;
    painter.draw_rect(position + Vec2f(handle_x, 0.0f) + Vec2f(m_handle_padding),
                      Vec2f(m_handle_width, preferred_size().y() - m_handle_padding * 2.0f),
                      Colour::from_srgb(0.11f, 0.64f, 0.92f));

    // Draw value label.
    m_value_label.paint(painter, position + (preferred_size() * 0.5f) - (m_value_label.preferred_size() * 0.5f));
}

void Slider::update(Vec2f mouse_position) {
    const float new_x = mouse_position.x() - (m_handle_width * 0.5f);
    const float new_ratio = new_x / (preferred_size().x() - m_handle_width);
    set_value(new_ratio * (m_max - m_min) + m_min);
}

bool Slider::handle_mouse_press(const MouseButtonEvent &event) {
    if (event.button() == MouseButton::Left) {
        tree().set_active_element(*this);
        update(event.position());
    }
    return true;
}

bool Slider::handle_mouse_release(const MouseButtonEvent &event) {
    if (event.button() == MouseButton::Left) {
        tree().unset_active_element();
    }
    return true;
}

bool Slider::handle_mouse_move(const MouseMoveEvent &event) {
    if (is_active_element()) {
        update(event.position());
    }
    return true;
}

void Slider::set_value(float value) {
    m_value = vull::clamp(value, m_min, m_max);
    m_value_label.set_text(vull::format("{}", m_value));
}

} // namespace vull::ui
