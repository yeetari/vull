#pragma once

#include <vull/vulkan/Buffer.hh>
#include <vull/vulkan/Image.hh>
#include <vull/vulkan/Pipeline.hh>
#include <vull/vulkan/RenderGraphDefs.hh>
#include <vull/vulkan/Vulkan.hh>

namespace vull::vk {

class Context;
class RenderGraph;

} // namespace vull::vk

namespace vull {

class DefaultRenderer;
struct Stream;

class SkyboxRenderer {
    vk::Context &m_context;
    vkb::DescriptorSetLayout m_set_layout;
    vk::Pipeline m_pipeline;
    vk::Image m_image;
    vk::Buffer m_descriptor_buffer;

public:
    SkyboxRenderer(vk::Context &context, DefaultRenderer &default_renderer);
    SkyboxRenderer(const SkyboxRenderer &) = delete;
    SkyboxRenderer(SkyboxRenderer &&) = delete;
    ~SkyboxRenderer();

    SkyboxRenderer &operator=(const SkyboxRenderer &) = delete;
    SkyboxRenderer &operator=(SkyboxRenderer &&) = delete;

    vk::ResourceId build_pass(vk::RenderGraph &graph, vk::ResourceId target, vk::ResourceId depth_image,
                              vk::ResourceId frame_ubo);
    void load(Stream &stream);
};

} // namespace vull
