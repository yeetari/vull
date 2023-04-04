#pragma once

#include <vull/ecs/EntityId.hh>
#include <vull/ecs/World.hh>
#include <vull/maths/Mat.hh>
#include <vull/support/HashMap.hh>
#include <vull/support/Optional.hh>
#include <vull/support/String.hh>
#include <vull/support/StringView.hh>
#include <vull/support/Vector.hh>
#include <vull/vulkan/Image.hh>

#include <stdint.h>

namespace vull::vk {

class Context;

} // namespace vull::vk

namespace vull {

struct Stream;

class Scene {
    vk::Context &m_context;
    World m_world;
    HashMap<String, uint32_t> m_texture_indices;
    Vector<vk::Image> m_images;
    Vector<vk::SampledImage> m_textures;

    vk::SampledImage load_texture(Stream &);

public:
    explicit Scene(vk::Context &context) : m_context(context) {}
    Scene(const Scene &) = delete;
    Scene(Scene &&) = delete;
    ~Scene() = default;

    Scene &operator=(const Scene &) = delete;
    Scene &operator=(Scene &&) = delete;

    Mat4f get_transform_matrix(EntityId entity);
    void load(StringView scene_name);

    World &world() { return m_world; }
    Optional<const uint32_t &> texture_index(const String &name) const { return m_texture_indices.get(name); }
    uint32_t texture_count() const { return m_textures.size(); }
    const Vector<vk::SampledImage> &textures() const { return m_textures; }
};

} // namespace vull
