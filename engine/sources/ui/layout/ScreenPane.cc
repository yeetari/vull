#include <vull/ui/layout/ScreenPane.hh>

#include <vull/container/Vector.hh>
#include <vull/support/Algorithm.hh>
#include <vull/support/Optional.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/ui/Element.hh>
#include <vull/ui/Units.hh>
#include <vull/ui/layout/Pane.hh>

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
