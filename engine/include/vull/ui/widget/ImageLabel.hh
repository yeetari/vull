#pragma once

#include <vull/support/Optional.hh>
#include <vull/ui/Element.hh>
#include <vull/ui/Units.hh> // IWYU pragma: keep

namespace vull::vk {

class Image;

} // namespace vull::vk

namespace vull::ui {

class Painter;
class Tree;

class ImageLabel : public Element {
    vk::Image &m_image;

public:
    ImageLabel(Tree &tree, Optional<Element &> parent, vk::Image &image);

    void paint(Painter &painter, LayoutPoint position) const override;
};

} // namespace vull::ui
