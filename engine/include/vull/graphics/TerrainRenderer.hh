#pragma once

#include <vull/maths/Vec.hh>
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

struct GBuffer;
class Terrain;

class TerrainRenderer {
    vk::Context &m_context;
    vkb::DescriptorSetLayout m_set_layout;
    vkb::DeviceSize m_set_layout_size;
    vk::Pipeline m_pipeline;
    vk::Pipeline m_wireframe_pipeline;
    vk::Buffer m_vertex_buffer;
    vk::Buffer m_index_buffer;
    vk::Image m_height_map;

    Vec3f m_view_position;
    bool m_wireframe{false};

public:
    explicit TerrainRenderer(vk::Context &context);
    TerrainRenderer(const TerrainRenderer &) = delete;
    TerrainRenderer(TerrainRenderer &&) = delete;
    ~TerrainRenderer();

    TerrainRenderer &operator=(const TerrainRenderer &) = delete;
    TerrainRenderer &operator=(TerrainRenderer &&) = delete;

    uint32_t build_pass(Terrain &terrain, vk::RenderGraph &graph, GBuffer &gbuffer, vk::ResourceId &frame_ubo);
    void load_heights(uint32_t seed);
    void set_view_position(const Vec3f &view_position) { m_view_position = view_position; }
    void set_wireframe(bool wireframe) { m_wireframe = wireframe; }
};

} // namespace vull
