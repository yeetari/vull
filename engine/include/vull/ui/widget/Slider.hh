#pragma once

#include <vull/support/Optional.hh>
#include <vull/ui/Element.hh>
#include <vull/ui/Units.hh>
#include <vull/ui/widget/Label.hh>

namespace vull::ui {

class Painter;
class MouseButtonEvent;
class MouseMoveEvent;
class Tree;

class Slider : public Element {
    const float m_min;
    const float m_max;
    Length m_handle_width{Length::make_cm(0.3f)};
    Length m_handle_padding{Length::make_absolute(LayoutUnit::from_int_pixels(3))};
    float m_value{0.0f};
    Label m_value_label;

    void update(LayoutPoint mouse_position);

public:
    Slider(Tree &tree, Optional<Element &> parent, float min, float max);

    void paint(Painter &painter, LayoutPoint position) const override;
    bool handle_mouse_press(const MouseButtonEvent &event) override;
    bool handle_mouse_release(const MouseButtonEvent &event) override;
    bool handle_mouse_move(const MouseMoveEvent &event) override;

    void set_handle_width(Length handle_width) { m_handle_width = handle_width; }
    void set_handle_padding(Length handle_padding) { m_handle_padding = handle_padding; }

    void set_value(float value);
    float value() const { return m_value; }
};

} // namespace vull::ui
