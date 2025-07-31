#pragma once

#include <vull/maths/vec.hh>
#include <vull/vulkan/pipeline.hh>
#include <vull/vulkan/vulkan.hh>

namespace vull::vk {

class Context;
class RenderGraph;
class ResourceId;

} // namespace vull::vk

namespace vull {

struct GBuffer;

class DeferredRenderer {
    vk::Context &m_context;

    vkb::DescriptorSetLayout m_set_layout;
    vkb::DeviceSize m_set_layout_size;

    vk::Pipeline m_light_cull_pipeline;
    vk::Pipeline m_deferred_pipeline;
    vk::Pipeline m_blit_tonemap_pipeline;
    float m_exposure{1.0f};

    void create_set_layouts();
    void create_pipelines();

public:
    explicit DeferredRenderer(vk::Context &context);
    DeferredRenderer(const DeferredRenderer &) = delete;
    DeferredRenderer(DeferredRenderer &&) = delete;
    ~DeferredRenderer();

    DeferredRenderer &operator=(const DeferredRenderer &) = delete;
    DeferredRenderer &operator=(DeferredRenderer &&) = delete;

    GBuffer create_gbuffer(vk::RenderGraph &graph, Vec2u viewport_extent);
    void build_pass(vk::RenderGraph &graph, GBuffer &gbuffer, vk::ResourceId &frame_ubo, vk::ResourceId &target);
    void set_exposure(float exposure) { m_exposure = exposure; }
};

} // namespace vull
