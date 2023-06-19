#pragma once

#include <vull/container/Vector.hh>
#include <vull/support/Optional.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/support/Utility.hh>
#include <vull/ui/Element.hh>
#include <vull/ui/Units.hh>

namespace vull::ui {

class Painter;
class Tree;

class Margins {
    Length m_top{Length::zero()};
    Length m_bottom{Length::zero()};
    Length m_left{Length::zero()};
    Length m_right{Length::zero()};

public:
    void set(Length top, Length bottom, Length left, Length right) {
        m_top = top;
        m_bottom = bottom;
        m_left = left;
        m_right = right;
    }
    void set_all(Length value) { set(value, value, value, value); }
    void set_top(Length top) { m_top = top; }
    void set_bottom(Length bottom) { m_bottom = bottom; }
    void set_left(Length left) { m_left = left; }
    void set_right(Length right) { m_right = right; }

    LayoutUnit main_axis_start(Tree &tree, Orientation orientation) const;
    LayoutUnit cross_axis_start(Tree &tree, Orientation orientation) const;
    LayoutUnit main_axis_total(Tree &tree, Orientation orientation) const;
    LayoutUnit cross_axis_total(Tree &tree, Orientation orientation) const;

    Length top() const { return m_top; }
    Length bottom() const { return m_bottom; }
    Length left() const { return m_left; }
    Length right() const { return m_right; }
};

class Pane : public Element {
    // TODO(small-vector)
    Vector<UniquePtr<Element>> m_children;
    Margins m_margins;

public:
    using Element::Element;

    bool is_pane() const override { return true; }

    template <typename T, typename... Args>
    T &add_child(Args &&...args);
    void clear_children();

    virtual void pre_layout(LayoutSize available_space) = 0;
    virtual void layout(LayoutSize available_space) = 0;

    Optional<HitResult> hit_test(LayoutPoint point) override;
    void paint(Painter &painter, LayoutPoint position) const override;

    const Vector<UniquePtr<Element>> &children() const { return m_children; }
    const Margins &margins() const { return m_margins; }
    Margins &margins() { return m_margins; }
};

template <typename T, typename... Args>
T &Pane::add_child(Args &&...args) {
    // NOLINTNEXTLINE
    auto *child = new T(tree(), *this, vull::forward<Args>(args)...);
    m_children.emplace(child);
    return *child;
}

} // namespace vull::ui
