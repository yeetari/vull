#pragma once

#include <vull/vulkan/Image.hh>
#include <vull/vulkan/Pipeline.hh>
#include <vull/vulkan/RenderGraphDefs.hh>
#include <vull/vulkan/Vulkan.hh>

namespace vull::vk {

class Context;
class RenderGraph;

} // namespace vull::vk

namespace vull::ui {

class Painter;

class Renderer {
    vk::Context &m_context;
    vkb::DescriptorSetLayout m_descriptor_set_layout{nullptr};
    vk::Pipeline m_pipeline;
    vk::Image m_null_image;

public:
    explicit Renderer(vk::Context &context);
    Renderer(const Renderer &) = delete;
    Renderer(Renderer &&) = delete;
    ~Renderer();

    Renderer &operator=(const Renderer &) = delete;
    Renderer &operator=(Renderer &&) = delete;

    void build_pass(vk::RenderGraph &graph, vk::ResourceId &target, Painter &&painter);
};

} // namespace vull::ui
