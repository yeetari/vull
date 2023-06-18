#include <vull/ui/widget/ImageLabel.hh>

#include <vull/support/Optional.hh>
#include <vull/ui/Element.hh>
#include <vull/ui/Painter.hh>
#include <vull/ui/Units.hh>
#include <vull/vulkan/Image.hh>
#include <vull/vulkan/Sampler.hh>
#include <vull/vulkan/Vulkan.hh>

namespace vull::ui {

ImageLabel::ImageLabel(Tree &tree, Optional<Element &> parent, vk::Image &image)
    : Element(tree, parent), m_image(image) {
    const auto width = Length::make_absolute(LayoutUnit::from_int_pixels(image.extent().width));
    const auto height = Length::make_absolute(LayoutUnit::from_int_pixels(image.extent().height));
    set_minimum_size({width, height});
    set_maximum_size({width, height});
}

void ImageLabel::paint(Painter &painter, LayoutPoint position) const {
    painter.draw_image(position, computed_size(), m_image.full_view().sampled(vk::Sampler::Nearest));
}

} // namespace vull::ui
