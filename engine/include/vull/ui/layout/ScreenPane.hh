#pragma once

#include <vull/maths/Vec.hh>
#include <vull/support/Optional.hh>
#include <vull/ui/layout/Pane.hh>

namespace vull::ui {

struct HitResult;

class ScreenPane final : public Pane {
public:
    using Pane::Pane;

    Optional<HitResult> hit_test(Vec2f point) override;
};

} // namespace vull::ui
