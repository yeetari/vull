#pragma once

#include <vull/maths/Vec.hh>
#include <vull/vulkan/Image.hh>
#include <vull/vulkan/Pipeline.hh>
#include <vull/vulkan/RenderGraphDefs.hh>
#include <vull/vulkan/Vulkan.hh>

namespace vull::vk {

class Context;
class RenderGraph;
class Shader;
class Swapchain;

} // namespace vull::vk

namespace vull::ui {

class CommandList;

class Renderer {
    vk::Context &m_context;
    const vk::Swapchain &m_swapchain;
    vkb::DescriptorSetLayout m_descriptor_set_layout{nullptr};
    vk::Pipeline m_pipeline;
    vk::Image m_null_image;
    Vec2f m_global_scale{96.0f / 2.54f};

public:
    Renderer(vk::Context &context, const vk::Swapchain &swapchain, const vk::Shader &vertex_shader,
             const vk::Shader &fragment_shader);
    Renderer(const Renderer &) = delete;
    Renderer(Renderer &&) = delete;
    ~Renderer();

    Renderer &operator=(const Renderer &) = delete;
    Renderer &operator=(Renderer &&) = delete;

    vk::ResourceId build_pass(vk::RenderGraph &graph, vk::ResourceId target, CommandList &&cmd_list);
    void set_global_scale(Vec2f global_scale) { m_global_scale = global_scale; }
    CommandList new_cmd_list();
};

} // namespace vull::ui
