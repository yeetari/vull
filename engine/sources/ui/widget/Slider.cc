#include <vull/ui/widget/Slider.hh>

#include <vull/core/Input.hh>
#include <vull/maths/Colour.hh>
#include <vull/maths/Common.hh>
#include <vull/support/Optional.hh>
#include <vull/support/StringBuilder.hh>
#include <vull/ui/Element.hh>
#include <vull/ui/Event.hh>
#include <vull/ui/Painter.hh>
#include <vull/ui/Tree.hh>
#include <vull/ui/Units.hh>
#include <vull/ui/widget/Label.hh>

namespace vull::ui {

Slider::Slider(Tree &tree, Optional<Element &> parent, float min, float max)
    : Element(tree, parent), m_min(min), m_max(max), m_value_label(tree, *this) {
    set_value(min);
}

void Slider::paint(Painter &painter, LayoutPoint position) const {
    auto colour = Colour::from_srgb(0.25f, 0.25f, 0.25f);
    if (is_hovered()) {
        colour = Colour::from_srgb(0.38f, 0.38f, 0.38f);
    }
    if (is_active_element()) {
        colour = Colour::from_srgb(0.67f, 0.67f, 0.67f, 0.39f);
    }

    // Draw groove.
    painter.paint_rect(position, computed_size(), colour);

    // Draw handle.
    const float value_ratio = (m_value - m_min) / (m_max - m_min);
    const LayoutUnit handle_width = m_handle_width.resolve(tree(), computed_width());
    const LayoutUnit handle_padding = m_handle_padding.resolve(tree(), computed_width());
    const LayoutUnit handle_x = (computed_width() - handle_width - handle_padding * 2).scale_by(value_ratio);
    painter.paint_rect(position + LayoutPoint(handle_x + handle_padding, handle_padding),
                       LayoutSize(handle_width, computed_height() - handle_padding * 2),
                       Colour::from_srgb(0.11f, 0.64f, 0.92f));

    // Draw value label.
    m_value_label.paint(painter, position + computed_size() / 2 - m_value_label.computed_size() / 2);
}

void Slider::update(LayoutPoint mouse_position) {
    const LayoutUnit handle_width = m_handle_width.resolve(tree(), computed_width());
    const LayoutUnit new_x = mouse_position.x() - (handle_width / 2);
    const float new_ratio = new_x.to_float() / (computed_width() - handle_width).to_float();
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

void Slider::handle_mouse_move(const MouseMoveEvent &event) {
    if (is_active_element()) {
        update(event.position());
    }
}

void Slider::set_value(float value) {
    m_value = vull::clamp(value, m_min, m_max);
    m_value_label.set_text(vull::format("{}", m_value));

    LayoutUnit handle_padding = m_handle_padding.resolve(tree());
    set_minimum_width(Length::make_absolute(m_value_label.computed_width() + handle_padding * 2));

    Length height = Length::make_absolute(m_value_label.computed_height() + handle_padding * 2);
    set_minimum_height(height);
    set_maximum_height(height);
}

} // namespace vull::ui
