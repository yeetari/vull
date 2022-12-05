#pragma once

#include <vull/ecs/EntityId.hh>
#include <vull/ecs/World.hh>
#include <vull/maths/Mat.hh>
#include <vull/support/HashMap.hh>
#include <vull/support/String.hh>
#include <vull/support/StringView.hh>
#include <vull/support/Vector.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stdint.h>

namespace vull::vk {

class Allocation;
class CommandBuffer;
class CommandPool;
class Context;
class Queue;

} // namespace vull::vk

namespace vull::vpak {

class ReadStream;

} // namespace vull::vpak

namespace vull {

struct PushConstantBlock {
    Mat4f transform;
    uint32_t albedo_index;
    uint32_t normal_index;
    uint32_t cascade_index;
};

class Scene {
    vk::Context &m_context;
    World m_world;
    Vector<vk::Allocation> m_allocations;
    HashMap<String, vkb::Buffer> m_vertex_buffers;
    HashMap<String, vkb::Buffer> m_index_buffers;
    HashMap<String, uint32_t> m_index_counts;
    HashMap<String, uint32_t> m_texture_indices;
    Vector<vkb::Image> m_texture_images;
    Vector<vkb::ImageView> m_texture_views;
    Vector<vkb::Sampler> m_texture_samplers;
    vkb::Sampler m_linear_sampler{nullptr};
    vkb::Sampler m_nearest_sampler{nullptr};

    vkb::Buffer load_buffer(vk::CommandPool &, vk::Queue &, vpak::ReadStream &, vkb::Buffer, void *, uint32_t,
                            vkb::BufferUsage);
    void load_image(vk::CommandPool &, vk::Queue &, vpak::ReadStream &, vkb::Buffer, void *);

public:
    explicit Scene(vk::Context &context) : m_context(context) {}
    Scene(const Scene &) = delete;
    Scene(Scene &&) = delete;
    ~Scene();

    Scene &operator=(const Scene &) = delete;
    Scene &operator=(Scene &&) = delete;

    Mat4f get_transform_matrix(EntityId entity);
    void load(vk::CommandPool &cmd_pool, vk::Queue &queue, StringView vpak_path, StringView scene_name);
    void render(vk::CommandBuffer &cmd_buf, uint32_t cascade_index);

    World &world() { return m_world; }
    uint32_t texture_count() const { return m_texture_images.size(); }
    const Vector<vkb::ImageView> &texture_views() const { return m_texture_views; }
    const Vector<vkb::Sampler> &texture_samplers() const { return m_texture_samplers; }
};

} // namespace vull
