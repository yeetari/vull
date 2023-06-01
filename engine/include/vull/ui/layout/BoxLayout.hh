#pragma once

#include <vull/support/Optional.hh>
#include <vull/ui/layout/Pane.hh>

#include <stdint.h>

namespace vull::ui {

class Element;
class Tree;

enum class Orientation : uint8_t {
    Horizontal,
    Vertical,
};

class BoxLayout : public Pane {
    float m_spacing{0.2f};
    const Orientation m_orientation;

public:
    BoxLayout(Tree &tree, Optional<Element &> parent, Orientation orientation)
        : Pane(tree, parent), m_orientation(orientation) {}

    void layout() override;
    void set_spacing(float spacing) { m_spacing = spacing; }
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
