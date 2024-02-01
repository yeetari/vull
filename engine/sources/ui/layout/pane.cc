#include <vull/ui/layout/pane.hh>

#include <vull/container/vector.hh>
#include <vull/support/optional.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/ui/element.hh>
#include <vull/ui/units.hh>

namespace vull::ui {

class Tree;

LayoutUnit Margins::main_axis_start(Tree &tree, Orientation orientation) const {
    if (orientation == Orientation::Horizontal) {
        return m_left.resolve(tree);
    }
    return m_top.resolve(tree);
}

LayoutUnit Margins::cross_axis_start(Tree &tree, Orientation orientation) const {
    if (orientation == Orientation::Vertical) {
        return m_left.resolve(tree);
    }
    return m_top.resolve(tree);
}

LayoutUnit Margins::main_axis_total(Tree &tree, Orientation orientation) const {
    if (orientation == Orientation::Horizontal) {
        return m_left.resolve(tree) + m_right.resolve(tree);
    }
    return m_top.resolve(tree) + m_bottom.resolve(tree);
}

LayoutUnit Margins::cross_axis_total(Tree &tree, Orientation orientation) const {
    if (orientation == Orientation::Vertical) {
        return m_left.resolve(tree) + m_right.resolve(tree);
    }
    return m_top.resolve(tree) + m_bottom.resolve(tree);
}

void Pane::clear_children() {
    m_children.clear();
}

Optional<HitResult> Pane::hit_test(LayoutPoint point) {
    if (!bounding_box_contains(point)) {
        return {};
    }
    for (const auto &child : m_children) {
        if (auto result = child->hit_test(point - child->offset_in_parent())) {
            return result;
        }
    }
    return HitResult{*this, point};
}

void Pane::paint(Painter &painter, LayoutPoint position) const {
    for (const auto &child : m_children) {
        child->paint(painter, position + child->offset_in_parent());
    }
}

} // namespace vull::ui
