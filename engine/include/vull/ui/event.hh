#pragma once

#include <vull/core/input.hh>
#include <vull/maths/vec.hh>
#include <vull/ui/units.hh>

namespace vull::ui {

class MouseEvent {
    LayoutPoint m_position;
    MouseButtonMask m_button_mask;

public:
    MouseEvent(LayoutPoint position, MouseButtonMask button_mask) : m_position(position), m_button_mask(button_mask) {}

    LayoutPoint position() const { return m_position; }
    MouseButtonMask button_mask() const { return m_button_mask; }
};

class MouseButtonEvent : public MouseEvent {
    MouseButton m_button;

public:
    MouseButtonEvent(LayoutPoint position, MouseButtonMask button_mask, MouseButton button)
        : MouseEvent(position, button_mask), m_button(button) {}

    MouseButton button() const { return m_button; }
};

class MouseMoveEvent : public MouseEvent {
    LayoutDelta m_delta;

public:
    MouseMoveEvent(LayoutPoint position, MouseButtonMask button_mask, LayoutDelta delta)
        : MouseEvent(position, button_mask), m_delta(delta) {}

    LayoutDelta delta() const { return m_delta; }
};

} // namespace vull::ui
