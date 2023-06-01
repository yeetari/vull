#include <vull/ui/layout/ScreenPane.hh>

#include <vull/container/Vector.hh>
#include <vull/maths/Vec.hh>
#include <vull/support/Optional.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/ui/Element.hh>

namespace vull::ui {

Optional<HitResult> ScreenPane::hit_test(Vec2f point) {
    for (const auto &child : children()) {
        if (auto result = child->hit_test(point - child->offset_in_parent())) {
            return result;
        }
    }
    return {};
}

} // namespace vull::ui
