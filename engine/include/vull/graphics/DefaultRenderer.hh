#pragma once

#include <vull/maths/Mat.hh>
#include <vull/maths/Vec.hh>
#include <vull/support/Array.hh>
#include <vull/support/HashMap.hh>
#include <vull/support/Optional.hh>
#include <vull/support/String.hh>
#include <vull/support/Vector.hh>
#include <vull/vulkan/Buffer.hh>
#include <vull/vulkan/Image.hh>
#include <vull/vulkan/Pipeline.hh>
#include <vull/vulkan/RenderGraph.hh>
#include <vull/vulkan/Shader.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stdint.h>

namespace vull::vk {

class CommandBuffer;
class Context;
class ImageView;
class QueryPool;
class Shader; // IWYU pragma: keep

} // namespace vull::vk

namespace vull::vpak {

class Reader;

} // namespace vull::vpak

namespace vull {

class Scene;

using ShaderMap = HashMap<String, vk::Shader>;

class DefaultRenderer {
    struct ShadowInfo {
        Array<Mat4f, 8> cascade_matrices;
        Array<float, 8> cascade_split_depths;
    };
    struct UniformBuffer {
        Mat4f proj;
        Mat4f cull_view;
        Mat4f view;
        Vec3f view_position;
        uint32_t object_count;
        Array<Vec4f, 4> frustum_planes;
        ShadowInfo shadow_info;
    };

    vk::Context &m_context;
    vkb::Extent2D m_tile_extent{};
    vkb::Extent3D m_viewport_extent{};

    vkb::DescriptorSetLayout m_static_set_layout;
    vkb::DescriptorSetLayout m_dynamic_set_layout;
    vkb::DescriptorSetLayout m_texture_set_layout;
    vkb::DescriptorSetLayout m_reduce_set_layout;
    vkb::DeviceSize m_static_set_layout_size{0};
    vkb::DeviceSize m_dynamic_set_layout_size{0};
    vkb::DeviceSize m_texture_set_layout_size{0};
    vkb::DeviceSize m_reduce_set_layout_size{0};

    vk::Image m_albedo_image;
    vk::Image m_normal_image;
    vk::Image m_depth_image;
    vkb::Extent2D m_depth_pyramid_extent;
    vk::Image m_depth_pyramid_image;
    Vector<vk::ImageView> m_depth_pyramid_views;
    vkb::Sampler m_depth_reduce_sampler;
    vk::Image m_shadow_map_image;
    Vector<vk::ImageView> m_shadow_cascade_views;
    vkb::Sampler m_shadow_sampler;
    vk::Buffer m_light_visibility_buffer;
    vk::Buffer m_object_visibility_buffer;
    vk::Buffer m_static_descriptor_buffer;
    vk::Buffer m_texture_descriptor_buffer;
    vk::Buffer m_vertex_buffer;
    vk::Buffer m_index_buffer;

    vk::Buffer m_dynamic_descriptor_buffer;
    vk::Buffer m_draw_buffer;
    uint32_t m_object_count{0};

    vk::Pipeline m_gbuffer_pipeline;
    vk::Pipeline m_shadow_pipeline;
    vk::Pipeline m_depth_reduce_pipeline;
    vk::Pipeline m_early_cull_pipeline;
    vk::Pipeline m_late_cull_pipeline;
    vk::Pipeline m_light_cull_pipeline;
    vk::Pipeline m_deferred_pipeline;

    vk::RenderGraph m_render_graph;
    vk::BufferResource *m_uniform_buffer_resource;
    vk::BufferResource *m_light_buffer_resource;
    vk::BufferResource *m_early_draw_buffer_resource;
    vk::BufferResource *m_late_draw_buffer_resource;
    vk::ImageResource *m_output_image_resource;

    Mat4f m_proj;
    Mat4f m_view;
    Vec3f m_view_position;
    vkb::ImageView m_output_view;
    ShadowInfo m_shadow_info;

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
    void create_pipelines(ShaderMap &&shader_map);
    void create_render_graph();
    uint8_t *put_descriptor(uint8_t *dst, vkb::DescriptorType type, void *info);
    void record_draws(vk::CommandBuffer &cmd_buf);
    void update_buffers(vk::CommandBuffer &cmd_buf);
    void update_cascades();

public:
    DefaultRenderer(vk::Context &context, ShaderMap &&shader_map, vkb::Extent3D viewport_extent);
    DefaultRenderer(const DefaultRenderer &) = delete;
    DefaultRenderer(DefaultRenderer &&) = delete;
    ~DefaultRenderer();

    DefaultRenderer &operator=(const DefaultRenderer &) = delete;
    DefaultRenderer &operator=(DefaultRenderer &&) = delete;

    void compile_render_graph();
    void load_scene(Scene &scene, vpak::Reader &pack_reader);
    void render(vk::CommandBuffer &cmd_buf, const Mat4f &proj, const Mat4f &view, const Vec3f &view_position,
                vkb::Image output_image, vkb::ImageView output_view,
                Optional<const vk::QueryPool &> timestamp_pool = {});
    void set_cull_view_locked(bool locked) { m_cull_view_locked = locked; }

    vk::RenderGraph &render_graph() { return m_render_graph; }
    vk::ImageResource &output_image_resource() { return *m_output_image_resource; }
};

} // namespace vull
