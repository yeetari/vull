#pragma once

#include <vull/core/Input.hh>
#include <vull/maths/Vec.hh>

namespace vull::ui {

class MouseEvent {
    Vec2f m_position;
    MouseButtonMask m_button_mask;

public:
    MouseEvent(Vec2f position, MouseButtonMask button_mask) : m_position(position), m_button_mask(button_mask) {}

    Vec2f position() const { return m_position; }
    MouseButtonMask button_mask() const { return m_button_mask; }
};

class MouseButtonEvent : public MouseEvent {
    MouseButton m_button;

public:
    MouseButtonEvent(Vec2f position, MouseButtonMask button_mask, MouseButton button)
        : MouseEvent(position, button_mask), m_button(button) {}

    MouseButton button() const { return m_button; }
};

class MouseMoveEvent : public MouseEvent {
    Vec2f m_delta;

public:
    MouseMoveEvent(Vec2f position, MouseButtonMask button_mask, Vec2f delta)
        : MouseEvent(position, button_mask), m_delta(delta) {}

    Vec2f delta() const { return m_delta; }
};

} // namespace vull::ui
