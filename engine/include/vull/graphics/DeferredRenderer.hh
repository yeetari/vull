#pragma once

#include <vull/vulkan/Pipeline.hh>
#include <vull/vulkan/RenderGraphDefs.hh>
#include <vull/vulkan/Vulkan.hh>

namespace vull::vk {

class Context;
class RenderGraph;

} // namespace vull::vk

namespace vull {

struct GBuffer;

class DeferredRenderer {
    vk::Context &m_context;
    vkb::Extent3D m_viewport_extent{};
    vkb::Extent3D m_tile_extent{};

    vkb::DescriptorSetLayout m_set_layout;
    vkb::DeviceSize m_set_layout_size;

    vk::Pipeline m_light_cull_pipeline;
    vk::Pipeline m_deferred_pipeline;
    vk::Pipeline m_blit_tonemap_pipeline;

    void create_set_layouts();
    void create_pipelines();

public:
    DeferredRenderer(vk::Context &context, vkb::Extent3D viewport_extent);
    DeferredRenderer(const DeferredRenderer &) = delete;
    DeferredRenderer(DeferredRenderer &&) = delete;
    ~DeferredRenderer();

    DeferredRenderer &operator=(const DeferredRenderer &) = delete;
    DeferredRenderer &operator=(DeferredRenderer &&) = delete;

    GBuffer create_gbuffer(vk::RenderGraph &graph);
    void build_pass(vk::RenderGraph &graph, GBuffer &gbuffer, vk::ResourceId &frame_ubo, vk::ResourceId &target);
};

} // namespace vull
