#pragma once

#include <vull/support/Optional.hh>
#include <vull/ui/Units.hh> // IWYU pragma: keep
#include <vull/ui/layout/Pane.hh>

namespace vull::ui {

struct HitResult;

class ScreenPane final : public Pane {
public:
    using Pane::Pane;

    // ^Element
    Optional<HitResult> hit_test(LayoutPoint point) override;

    // ^Pane
    void pre_layout(LayoutSize available_space) override;
    void layout(LayoutSize available_space) override;
};

} // namespace vull::ui
