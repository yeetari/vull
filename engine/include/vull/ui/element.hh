#pragma once

#include <vull/support/flag_bitset.hh>
#include <vull/support/optional.hh>
#include <vull/ui/units.hh>

#include <stdint.h>

namespace vull::ui {

class Painter;
class Element;
class MouseEvent;
class MouseButtonEvent;
class MouseMoveEvent;
class Style;
class Tree;

struct HitResult {
    Element &element;
    LayoutPoint relative_position;
};

enum class Align : uint8_t {
    Left,
    Center,
    Right,
};

enum class ElementFlag : uint8_t {
    None = 0,
    Visible,
};

using ElementFlags = FlagBitset<ElementFlag>;

class Element {
    Tree &m_tree;
    Optional<Element &> m_parent;

    Size m_minimum_size{Length::shrink(), Length::shrink()};
    Size m_maximum_size{Length::grow(), Length::grow()};
    Align m_align{Align::Left};

    LayoutPoint m_offset_in_parent;
    LayoutSize m_computed_size;
    ElementFlags m_flags{ElementFlag::Visible};

public:
    Element(Tree &tree, Optional<Element &> parent) : m_tree(tree), m_parent(parent) {}
    Element(const Element &) = delete;
    Element(Element &&) = delete;
    virtual ~Element();

    Element &operator=(const Element &) = delete;
    Element &operator=(Element &&) = delete;

    bool bounding_box_contains(LayoutPoint point) const;
    virtual Optional<HitResult> hit_test(LayoutPoint point);
    virtual void paint(Painter &painter, LayoutPoint position) const = 0;

    void set_minimum_size(const Size &size) { m_minimum_size = size; }
    void set_minimum_width(Length width) { m_minimum_size.set_width(width); }
    void set_minimum_height(Length height) { m_minimum_size.set_height(height); }

    void set_maximum_size(const Size &size) { m_maximum_size = size; }
    void set_maximum_width(Length width) { m_maximum_size.set_width(width); }
    void set_maximum_height(Length height) { m_maximum_size.set_height(height); }

    void set_align(Align align) { m_align = align; }

    void set_computed_size(LayoutSize size) { m_computed_size = size; }
    void set_computed_width(LayoutUnit width) { m_computed_size.set_width(width); }
    void set_computed_height(LayoutUnit height) { m_computed_size.set_height(height); }
    void set_offset_in_parent(LayoutPoint point) { m_offset_in_parent = point; }

    virtual bool handle_mouse_press(const MouseButtonEvent &) { return false; }
    virtual bool handle_mouse_release(const MouseButtonEvent &) { return false; }
    virtual void handle_mouse_move(const MouseMoveEvent &) {}
    virtual void handle_mouse_enter(const MouseEvent &) {}
    virtual void handle_mouse_exit(const MouseEvent &) {}

    virtual bool is_pane() const { return false; }
    virtual bool is_screen_pane() const { return false; }

    void set_visible(bool visible);
    bool is_active_element() const;
    bool is_visible() const;
    bool is_hovered() const;

    LayoutSize computed_size() const { return m_computed_size; }
    LayoutUnit computed_width() const { return m_computed_size.width(); }
    LayoutUnit computed_height() const { return m_computed_size.height(); }

    Style &style() const;
    Tree &tree() const { return m_tree; }
    Optional<Element &> parent() const { return m_parent; }
    const Size &minimum_size() const { return m_minimum_size; }
    const Size &maximum_size() const { return m_maximum_size; }
    Align align() const { return m_align; }
    LayoutPoint offset_in_parent() const { return m_offset_in_parent; }
    ElementFlags flags() const { return m_flags; }
};

} // namespace vull::ui
