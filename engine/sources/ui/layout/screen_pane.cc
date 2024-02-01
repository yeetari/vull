#include <vull/ui/layout/screen_pane.hh>

#include <vull/container/vector.hh>
#include <vull/support/algorithm.hh>
#include <vull/support/optional.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/ui/element.hh>
#include <vull/ui/layout/pane.hh>
#include <vull/ui/units.hh>

namespace vull::ui {

void ScreenPane::bring_to_front(Element &element) {
    // NOLINTNEXTLINE
    auto it = vull::find_if(children().begin(), children().end(), [&](const auto &elem) {
        return elem.ptr() == &element;
    });
    if (it != children().end()) {
        vull::rotate(it, it + 1, children().end());
    }
}

Optional<HitResult> ScreenPane::hit_test(LayoutPoint point) {
    for (const auto &child : vull::reverse_view(children())) {
        if (auto result = child->hit_test(point - child->offset_in_parent())) {
            return result;
        }
    }
    return {};
}

void ScreenPane::pre_layout(LayoutSize) {
    for (const auto &child : children()) {
        if (child->is_pane()) {
            static_cast<Pane &>(*child).pre_layout({});
        }
    }
}

void ScreenPane::layout(LayoutSize) {
    for (const auto &child : children()) {
        if (child->is_pane()) {
            static_cast<Pane &>(*child).layout({});
        }
    }
}

} // namespace vull::ui
