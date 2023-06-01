#pragma once

#include <vull/maths/Vec.hh>
#include <vull/vulkan/Image.hh>
#include <vull/vulkan/Pipeline.hh>
#include <vull/vulkan/RenderGraphDefs.hh>
#include <vull/vulkan/Vulkan.hh>

namespace vull::vk {

class Context;
class RenderGraph;

} // namespace vull::vk

namespace vull::ui {

class CommandList;

class Renderer {
    vk::Context &m_context;
    vkb::DescriptorSetLayout m_descriptor_set_layout{nullptr};
    vk::Pipeline m_pipeline;
    vk::Image m_null_image;
    Vec2f m_global_scale;

public:
    Renderer(vk::Context &context, Vec2f global_scale);
    Renderer(const Renderer &) = delete;
    Renderer(Renderer &&) = delete;
    ~Renderer();

    Renderer &operator=(const Renderer &) = delete;
    Renderer &operator=(Renderer &&) = delete;

    vk::ResourceId build_pass(vk::RenderGraph &graph, vk::ResourceId target, CommandList &&cmd_list);
    CommandList new_cmd_list();
};

} // namespace vull::ui
