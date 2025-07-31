#pragma once

#include <vull/container/array.hh>
#include <vull/container/hash_map.hh>
#include <vull/graphics/texture_streamer.hh>
#include <vull/maths/mat.hh>
#include <vull/maths/vec.hh>
#include <vull/support/string.hh>
#include <vull/support/string_view.hh>
#include <vull/vulkan/buffer.hh>
#include <vull/vulkan/pipeline.hh>
#include <vull/vulkan/vulkan.hh>

#include <stdint.h>

namespace vull::vk {

class CommandBuffer;
class Context;
class RenderGraph;
class ResourceId;

} // namespace vull::vk

namespace vull {

class Camera;
struct GBuffer;
class Scene;

class DefaultRenderer {
    vk::Context &m_context;
    TextureStreamer m_texture_streamer;

    vkb::DescriptorSetLayout m_main_set_layout;
    vkb::DescriptorSetLayout m_reduce_set_layout;
    vkb::DeviceSize m_main_set_layout_size{0};
    vkb::DeviceSize m_reduce_set_layout_size{0};

    vk::Buffer m_object_visibility_buffer;
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
    void update_ubo(const vk::Buffer &buffer, Vec2u viewport_extent);
    void record_draws(vk::CommandBuffer &cmd_buf, const vk::Buffer &draw_buffer);

public:
    explicit DefaultRenderer(vk::Context &context);
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
