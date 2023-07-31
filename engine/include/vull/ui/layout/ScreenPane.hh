#pragma once

#include <vull/support/Optional.hh>
#include <vull/ui/Units.hh> // IWYU pragma: keep
#include <vull/ui/layout/Pane.hh>

namespace vull::ui {

class Element;
struct HitResult;

class ScreenPane final : public Pane {
public:
    using Pane::Pane;

    bool is_screen_pane() const override { return true; }

    void bring_to_front(Element &element);

    // ^Element
    Optional<HitResult> hit_test(LayoutPoint point) override;

    // ^Pane
    void pre_layout(LayoutSize available_space) override;
    void layout(LayoutSize available_space) override;
};

} // namespace vull::ui
