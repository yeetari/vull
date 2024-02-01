#pragma once

#include <vull/core/input.hh>
#include <vull/maths/vec.hh>
#include <vull/support/optional.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>
#include <vull/ui/element.hh>
#include <vull/ui/units.hh>

namespace vull::ui {

class Painter;
class Style;

class Tree {
    Style &m_style;
    Vec2f m_ppcm;
    UniquePtr<Element> m_root_element;
    Optional<Element &> m_active_element;
    Optional<Element &> m_hovered_element;
    LayoutPoint m_hovered_relative_position;
    LayoutPoint m_mouse_position;
    MouseButtonMask m_mouse_buttons{};
    bool m_need_hover_update{false};

    void update_hover();
    template <auto>
    void handle_mouse_press_release(MouseButton button);

public:
    Tree(Style &style, Vec2f ppcm) : m_style(style), m_ppcm(ppcm) {}

    template <typename T, typename... Args>
    T &set_root(Args &&...args);
    void render(Painter &painter);

    void handle_element_destruct(Element &element);
    void set_active_element(Element &element);
    void unset_active_element();

    void handle_mouse_press(MouseButton button);
    void handle_mouse_release(MouseButton button);
    void handle_mouse_move(Vec2i delta, Vec2u position, MouseButtonMask buttons);

    Style &style() const { return m_style; }
    Vec2f ppcm() const { return m_ppcm; }
    Optional<Element &> active_element() const { return m_active_element; }
};

template <typename T, typename... Args>
T &Tree::set_root(Args &&...args) {
    auto *root = new T(*this, vull::nullopt, vull::forward<Args>(args)...);
    m_root_element = vull::adopt_unique(root);
    return *root;
}

} // namespace vull::ui
