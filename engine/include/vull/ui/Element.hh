#pragma once

#include <vull/maths/Vec.hh>
#include <vull/support/Enum.hh>
#include <vull/support/Optional.hh>

#include <stdint.h>

namespace vull::ui {

class CommandList;
class Element;
class MouseEvent;
class MouseButtonEvent;
class MouseMoveEvent;
class Tree;

struct HitResult {
    Element &element;
    Vec2f relative_position;
};

enum class ElementFlags : uint8_t {
    None = 0,
    Hovered = 1u << 0u,
    RightAlign = 1u << 1u,
};

class Element {
    Tree &m_tree;
    Optional<Element &> m_parent;
    Vec2f m_offset_in_parent{};
    Vec2f m_preferred_size{};
    ElementFlags m_flags{};

public:
    Element(Tree &tree, Optional<Element &> parent) : m_tree(tree), m_parent(parent) {}
    Element(const Element &) = delete;
    Element(Element &&) = delete;
    virtual ~Element();

    Element &operator=(const Element &) = delete;
    Element &operator=(Element &&) = delete;

    bool bounding_box_contains(Vec2f point) const;
    virtual Optional<HitResult> hit_test(Vec2f point);
    virtual void paint(CommandList &cmd_list, Vec2f position) const = 0;

    void set_offset_in_parent(Vec2f offset) { m_offset_in_parent = offset; }
    void set_preferred_size(Vec2f size) { m_preferred_size = size; }
    void set_right_align(bool right_align);

    virtual bool handle_mouse_press(const MouseButtonEvent &) { return false; }
    virtual bool handle_mouse_release(const MouseButtonEvent &) { return false; }
    virtual bool handle_mouse_move(const MouseMoveEvent &) { return false; }
    virtual bool handle_mouse_enter(const MouseEvent &);
    virtual bool handle_mouse_exit(const MouseEvent &);
    virtual bool is_pane() const { return false; }

    bool is_active_element() const;
    bool is_hovered() const;

    Tree &tree() const { return m_tree; }
    Optional<Element &> parent() const { return m_parent; }
    Vec2f offset_in_parent() const { return m_offset_in_parent; }
    Vec2f preferred_size() const { return m_preferred_size; }
    ElementFlags flags() const { return m_flags; }
};

VULL_DEFINE_FLAG_ENUM_OPS(ElementFlags)

} // namespace vull::ui
