#pragma once

#include <vull/maths/Vec.hh>
#include <vull/support/Optional.hh>
#include <vull/ui/Element.hh>
#include <vull/ui/widget/Label.hh>

namespace vull::ui {

class Painter;
class Font;
class MouseButtonEvent;
class MouseMoveEvent;
class Tree;

class Slider : public Element {
    const float m_min;
    const float m_max;
    float m_handle_width{0.3f};
    float m_handle_padding{0.05f};
    float m_value{0.0f};
    Label m_value_label;

    void update(Vec2f mouse_position);

public:
    Slider(Tree &tree, Optional<Element &> parent, Font &font, float min, float max);

    void paint(Painter &painter, Vec2f position) const override;
    bool handle_mouse_press(const MouseButtonEvent &event) override;
    bool handle_mouse_release(const MouseButtonEvent &event) override;
    bool handle_mouse_move(const MouseMoveEvent &event) override;

    void set_handle_width(float handle_width) { m_handle_width = handle_width; }
    void set_handle_padding(float handle_padding) { m_handle_padding = handle_padding; }

    void set_value(float value);
    float value() const { return m_value; }
};

} // namespace vull::ui
