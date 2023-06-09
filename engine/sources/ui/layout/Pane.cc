#include <vull/ui/layout/Pane.hh>

#include <vull/container/Vector.hh>
#include <vull/maths/Vec.hh>
#include <vull/support/Optional.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/ui/Element.hh>

namespace vull::ui {

void Pane::clear_children() {
    m_children.clear();
}

void Pane::layout() {
    for (const auto &child : m_children) {
        if (child->is_pane()) {
            static_cast<Pane &>(*child).layout();
        }
    }
}

Optional<HitResult> Pane::hit_test(Vec2f point) {
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

void Pane::paint(Painter &painter, Vec2f position) const {
    for (const auto &child : m_children) {
        child->paint(painter, position + child->offset_in_parent());
    }
}

} // namespace vull::ui
