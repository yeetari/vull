#include <vull/ui/widget/image_label.hh>

#include <vull/support/optional.hh>
#include <vull/ui/element.hh>
#include <vull/ui/painter.hh>
#include <vull/ui/units.hh>
#include <vull/vulkan/image.hh>
#include <vull/vulkan/sampler.hh>
#include <vull/vulkan/vulkan.hh>

namespace vull::ui {

ImageLabel::ImageLabel(Tree &tree, Optional<Element &> parent, vk::Image &image)
    : Element(tree, parent), m_image(image) {
    const auto width = Length::make_absolute(LayoutUnit::from_int_pixels(image.extent().width));
    const auto height = Length::make_absolute(LayoutUnit::from_int_pixels(image.extent().height));
    set_minimum_size({width, height});
    set_maximum_size({width, height});
}

void ImageLabel::paint(Painter &painter, LayoutPoint position) const {
    painter.paint_image(position, computed_size(), m_image.full_view().sampled(vk::Sampler::Nearest));
}

} // namespace vull::ui
