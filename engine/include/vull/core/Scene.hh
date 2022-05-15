#pragma once

#include <vull/ecs/EntityId.hh>
#include <vull/ecs/World.hh>
#include <vull/maths/Mat.hh>
#include <vull/support/Vector.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stdint.h>
#include <stdio.h>

namespace vull {

class CommandBuffer;
class CommandPool;
class PackReader;
class Queue;
class VkContext;

struct PushConstantBlock {
    Mat4f transform;
    uint32_t albedo_index;
    uint32_t normal_index;
    uint32_t cascade_index;
};

class Scene {
    VkContext &m_context;
    World m_world;
    vkb::DeviceMemory m_memory{nullptr};
    Vector<vkb::Buffer> m_vertex_buffers;
    Vector<vkb::Buffer> m_index_buffers;
    Vector<vkb::Image> m_texture_images;
    Vector<vkb::ImageView> m_texture_views;

    vkb::Buffer load_buffer(CommandPool &, Queue &, PackReader &, vkb::Buffer, void *, vkb::DeviceSize &, uint32_t,
                            vkb::BufferUsage);
    void load_image(CommandPool &, Queue &, PackReader &, vkb::Buffer, void *, vkb::DeviceSize &);

public:
    explicit Scene(VkContext &context) : m_context(context) {}
    Scene(const Scene &) = delete;
    Scene(Scene &&) = delete;
    ~Scene();

    Scene &operator=(const Scene &) = delete;
    Scene &operator=(Scene &&) = delete;

    Mat4f get_transform_matrix(EntityId entity);
    void load(CommandPool &cmd_pool, Queue &queue, FILE *pack_file);
    void render(const CommandBuffer &cmd_buf, vkb::PipelineLayout pipeline_layout, uint32_t cascade_index);

    uint32_t texture_count() const { return m_texture_images.size(); }
    const Vector<vkb::ImageView> &texture_views() const { return m_texture_views; }
};

} // namespace vull
