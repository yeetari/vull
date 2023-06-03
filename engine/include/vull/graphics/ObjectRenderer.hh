#pragma once

#include <vull/vulkan/Buffer.hh>
#include <vull/vulkan/Pipeline.hh>
#include <vull/vulkan/RenderGraphDefs.hh>

namespace vull::vk {

class Context;
class RenderGraph;

} // namespace vull::vk

namespace vull {

class Mesh;

class ObjectRenderer {
    vk::Context &m_context;
    vk::Pipeline m_pipeline;
    vk::Buffer m_vertex_buffer;
    vk::Buffer m_index_buffer;
    float m_rotation{0.0f};

public:
    explicit ObjectRenderer(vk::Context &context);

    void build_pass(vk::RenderGraph &graph, vk::ResourceId &target);

    void load(const Mesh &mesh);
    void set_rotation(float rotation) { m_rotation = rotation; }
};

} // namespace vull
