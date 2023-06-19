#pragma once

#include <vull/support/Optional.hh>
#include <vull/ui/Units.hh>
#include <vull/ui/layout/Pane.hh>

namespace vull::ui {

class Element;
class Tree;

class BoxLayout : public Pane {
    Length m_spacing{Length::make_cm(0.2f)};
    const Orientation m_orientation;

    void set_computed_main_axis(LayoutUnit length);
    void set_computed_cross_axis(LayoutUnit length);
    LayoutUnit computed_main_axis() const;
    LayoutUnit computed_cross_axis() const;

public:
    BoxLayout(Tree &tree, Optional<Element &> parent, Orientation orientation)
        : Pane(tree, parent), m_orientation(orientation) {}

    void pre_layout(LayoutSize available_space) override;
    void layout(LayoutSize available_space) override;
    void set_spacing(const Length &spacing) { m_spacing = spacing; }
};

class HBoxLayout : public BoxLayout {
public:
    HBoxLayout(Tree &tree, Optional<Element &> parent) : BoxLayout(tree, parent, Orientation::Horizontal) {}
};

class VBoxLayout : public BoxLayout {
public:
    VBoxLayout(Tree &tree, Optional<Element &> parent) : BoxLayout(tree, parent, Orientation::Vertical) {}
};

} // namespace vull::ui
