#pragma once

#include <vull/vulkan/RenderGraph.hh>
#include <vull/vulkan/Vulkan.hh>

namespace vull::vk {

class Swapchain;

} // namespace vull::vk

namespace vull {

class RenderEngine : public vk::RenderGraph {
    vk::ImageResource *m_output_image;

public:
    RenderEngine();

    void compile();
    void link_swapchain(const vk::Swapchain &swapchain);
    void set_output(vkb::Image image, vkb::ImageView view);
    vk::ImageResource &output_image() const { return *m_output_image; }
};

} // namespace vull
