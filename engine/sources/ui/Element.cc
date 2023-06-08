#include <vull/ui/Element.hh>

#include <vull/maths/Relational.hh>
#include <vull/maths/Vec.hh>
#include <vull/support/Optional.hh>
#include <vull/support/Utility.hh>
#include <vull/ui/Tree.hh>

namespace vull::ui {

Element::~Element() {
    m_tree.handle_element_destruct(*this);
}

bool Element::bounding_box_contains(Vec2f point) const {
    if (vull::any(vull::less_than(point, Vec2f(0.0f)))) {
        return false;
    }
    if (vull::any(vull::greater_than(point, m_preferred_size))) {
        return false;
    }
    return true;
}

Optional<HitResult> Element::hit_test(Vec2f point) {
    if (bounding_box_contains(point)) {
        return HitResult{*this, point};
    }
    return {};
}

void Element::set_right_align(bool right_align) {
    if (right_align) {
        m_flags |= ElementFlags::RightAlign;
    } else {
        m_flags &= ~ElementFlags::RightAlign;
    }
}

bool Element::handle_mouse_enter(const MouseEvent &) {
    m_flags |= ElementFlags::Hovered;
    return true;
}

bool Element::handle_mouse_exit(const MouseEvent &) {
    m_flags &= ~ElementFlags::Hovered;
    return true;
}

bool Element::is_active_element() const {
    return m_tree.active_element() && m_tree.active_element().ptr() == this;
}

bool Element::is_hovered() const {
    return (m_flags & ElementFlags::Hovered) != ElementFlags::None;
}

} // namespace vull::ui
