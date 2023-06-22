#pragma once

#include <vull/container/Array.hh>
#include <vull/container/HashMap.hh>
#include <vull/maths/Mat.hh>
#include <vull/maths/Vec.hh>
#include <vull/support/String.hh>
#include <vull/vulkan/Buffer.hh>
#include <vull/vulkan/Pipeline.hh>
#include <vull/vulkan/RenderGraphDefs.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stdint.h>

namespace vull::vk {

class CommandBuffer;
class Context;
class RenderGraph;
class Shader; // IWYU pragma: keep

} // namespace vull::vk

namespace vull {

class Camera;
struct GBuffer;
class Scene;

class DefaultRenderer {
    vk::Context &m_context;
    vkb::Extent3D m_viewport_extent{};

    vkb::DescriptorSetLayout m_main_set_layout;
    vkb::DescriptorSetLayout m_texture_set_layout;
    vkb::DescriptorSetLayout m_reduce_set_layout;
    vkb::DeviceSize m_main_set_layout_size{0};
    vkb::DeviceSize m_texture_set_layout_size{0};
    vkb::DeviceSize m_reduce_set_layout_size{0};

    vkb::Extent2D m_depth_pyramid_extent;
    vk::Buffer m_object_visibility_buffer;
    vk::Buffer m_texture_descriptor_buffer;
    vk::Buffer m_vertex_buffer;
    vk::Buffer m_index_buffer;
    uint32_t m_object_count{0};

    vk::Pipeline m_gbuffer_pipeline;
    vk::Pipeline m_shadow_pipeline;
    vk::Pipeline m_depth_reduce_pipeline;
    vk::Pipeline m_early_cull_pipeline;
    vk::Pipeline m_late_cull_pipeline;

    const Camera *m_camera{nullptr};
    Mat4f m_cull_view;
    Array<Vec4f, 4> m_frustum_planes;
    bool m_cull_view_locked{false};

    struct MeshInfo {
        uint32_t index_count;
        uint32_t index_offset;
        int32_t vertex_offset;
    };
    HashMap<String, MeshInfo> m_mesh_infos;
    Scene *m_scene{nullptr};

    void create_set_layouts();
    void create_resources();
    void create_pipelines();
    void update_ubo(const vk::Buffer &buffer);
    void record_draws(vk::CommandBuffer &cmd_buf, const vk::Buffer &draw_buffer);

public:
    DefaultRenderer(vk::Context &context, vkb::Extent3D viewport_extent);
    DefaultRenderer(const DefaultRenderer &) = delete;
    DefaultRenderer(DefaultRenderer &&) = delete;
    ~DefaultRenderer();

    DefaultRenderer &operator=(const DefaultRenderer &) = delete;
    DefaultRenderer &operator=(DefaultRenderer &&) = delete;

    vk::ResourceId build_pass(vk::RenderGraph &graph, GBuffer &gbuffer);
    void load_scene(Scene &scene);
    void set_camera(const Camera &camera) { m_camera = &camera; }
    void set_cull_view_locked(bool locked) { m_cull_view_locked = locked; }
};

} // namespace vull
