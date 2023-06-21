#include <vull/ui/Element.hh>

#include <vull/support/Optional.hh>
#include <vull/support/Utility.hh>
#include <vull/ui/Tree.hh>
#include <vull/ui/Units.hh>

namespace vull::ui {

Element::~Element() {
    m_tree.handle_element_destruct(*this);
}

bool Element::bounding_box_contains(LayoutPoint point) const {
    if (point.x() < 0 || point.y() < 0) {
        return false;
    }
    if (point.x() > computed_width() || point.y() > computed_height()) {
        return false;
    }
    return true;
}

Optional<HitResult> Element::hit_test(LayoutPoint point) {
    if (bounding_box_contains(point)) {
        return HitResult{*this, point};
    }
    return {};
}

void Element::handle_mouse_enter(const MouseEvent &) {
    m_flags |= ElementFlags::Hovered;
}

void Element::handle_mouse_exit(const MouseEvent &) {
    m_flags &= ~ElementFlags::Hovered;
}

bool Element::is_active_element() const {
    return m_tree.active_element() && m_tree.active_element().ptr() == this;
}

bool Element::is_hovered() const {
    return (m_flags & ElementFlags::Hovered) != ElementFlags::None;
}

Style &Element::style() const {
    return tree().style();
}

} // namespace vull::ui
