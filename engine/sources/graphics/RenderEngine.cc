#include <vull/graphics/RenderEngine.hh>

#include <vull/vulkan/RenderGraph.hh>
#include <vull/vulkan/Vulkan.hh>

namespace vull {

RenderEngine::RenderEngine() {
    m_output_image = &add_image("output-image");
}

void RenderEngine::compile() {
    vk::RenderGraph::compile(*m_output_image);
}

void RenderEngine::link_swapchain(const vk::Swapchain &) {
    m_output_image->set_range({
        .aspectMask = vkb::ImageAspect::Color,
        .levelCount = 1,
        .layerCount = 1,
    });
}

void RenderEngine::set_output(vkb::Image image, vkb::ImageView view) {
    m_output_image->set(image, view);
}

} // namespace vull
