#pragma once

#include <vull/vulkan/image.hh>
#include <vull/vulkan/pipeline.hh>
#include <vull/vulkan/vulkan.hh>

namespace vull::vk {

class Context;
class RenderGraph;
class ResourceId;

} // namespace vull::vk

namespace vull::ui {

class Painter;

class Renderer {
    vk::Context &m_context;
    vkb::DescriptorSetLayout m_descriptor_set_layout{nullptr};
    vk::Pipeline m_pipeline;
    vk::Image m_null_image;
    vk::Image m_shadow_image;

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
