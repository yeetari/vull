#pragma once

#include <vull/core/Input.hh>
#include <vull/maths/Vec.hh>
#include <vull/support/Optional.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/support/Utility.hh>
#include <vull/ui/Element.hh>

namespace vull::ui {

class Painter;
class Style;

class Tree {
    Style &m_style;
    Vec2f m_global_scale;
    UniquePtr<Element> m_root_element;
    Optional<Element &> m_active_element;
    Optional<Element &> m_hovered_element;
    Vec2f m_hovered_relative_position;
    Vec2f m_mouse_position;
    MouseButtonMask m_mouse_buttons{};
    bool m_need_hover_update{false};

    void update_hover();

public:
    Tree(Style &style, Vec2f global_scale) : m_style(style), m_global_scale(global_scale) {}

    template <typename T, typename... Args>
    T &set_root(Args &&...args);
    void render(Painter &painter);

    void handle_element_destruct(Element &element);
    void set_active_element(Element &element);
    void unset_active_element();

    void handle_mouse_press(MouseButton button);
    void handle_mouse_release(MouseButton button);
    void handle_mouse_move(Vec2f delta, Vec2f position, MouseButtonMask buttons);

    Style &style() const { return m_style; }
    Vec2f global_scale() const { return m_global_scale; }
    Optional<Element &> active_element() const { return m_active_element; }
};

template <typename T, typename... Args>
T &Tree::set_root(Args &&...args) {
    auto *root = new T(*this, Optional<Element &>(), vull::forward<Args>(args)...);
    m_root_element = vull::adopt_unique(root);
    return *root;
}

} // namespace vull::ui
