#pragma once

#include <vull/vulkan/image.hh>
#include <vull/vulkan/pipeline.hh>
#include <vull/vulkan/vulkan.hh>

namespace vull::vk {

class Context;
class RenderGraph;
class ResourceId;

} // namespace vull::vk

namespace vull {

struct Stream;

class SkyboxRenderer {
    vk::Context &m_context;
    vkb::DescriptorSetLayout m_set_layout;
    vkb::DeviceSize m_set_layout_size;
    vk::Pipeline m_pipeline;
    vk::Image m_image;

public:
    explicit SkyboxRenderer(vk::Context &context);
    SkyboxRenderer(const SkyboxRenderer &) = delete;
    SkyboxRenderer(SkyboxRenderer &&) = delete;
    ~SkyboxRenderer();

    SkyboxRenderer &operator=(const SkyboxRenderer &) = delete;
    SkyboxRenderer &operator=(SkyboxRenderer &&) = delete;

    void build_pass(vk::RenderGraph &graph, vk::ResourceId &depth_image, vk::ResourceId &frame_ubo,
                    vk::ResourceId &target);
    void load(Stream &stream);
};

} // namespace vull
