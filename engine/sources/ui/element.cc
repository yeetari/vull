#include <vull/ui/element.hh>

#include <vull/support/optional.hh>
#include <vull/support/utility.hh>
#include <vull/ui/tree.hh>
#include <vull/ui/units.hh>

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

bool Element::is_active_element() const {
    return m_tree.active_element().ptr() == this;
}

bool Element::is_hovered() const {
    return m_tree.hovered_element().ptr() == this;
}

Style &Element::style() const {
    return m_tree.style();
}

} // namespace vull::ui
