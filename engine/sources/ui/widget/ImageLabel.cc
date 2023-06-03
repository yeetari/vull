#include <vull/ui/widget/ImageLabel.hh>

#include <vull/maths/Vec.hh>
#include <vull/support/Optional.hh>
#include <vull/ui/Element.hh>
#include <vull/ui/Painter.hh>
#include <vull/ui/Tree.hh>
#include <vull/vulkan/Image.hh>
#include <vull/vulkan/Sampler.hh>
#include <vull/vulkan/Vulkan.hh>

namespace vull::ui {

ImageLabel::ImageLabel(Tree &tree, Optional<Element &> parent, vk::Image &image)
    : Element(tree, parent), m_image(image) {
    set_preferred_size(Vec2f(image.extent().width, image.extent().height) / tree.global_scale());
}

void ImageLabel::paint(Painter &painter, Vec2f position) const {
    painter.draw_image(position, preferred_size(), m_image.full_view().sampled(vk::Sampler::Nearest));
}

} // namespace vull::ui
