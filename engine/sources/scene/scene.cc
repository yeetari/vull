#include <vull/scene/scene.hh>

#include <vull/core/bounding_box.hh>
#include <vull/core/bounding_sphere.hh>
#include <vull/ecs/entity_id.hh>
#include <vull/ecs/world.hh>
#include <vull/graphics/material.hh>
#include <vull/graphics/mesh.hh>
#include <vull/maths/mat.hh>
#include <vull/scene/transform.hh>
#include <vull/support/result.hh>
#include <vull/support/string_view.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>
#include <vull/vpak/file_system.hh>
#include <vull/vpak/reader.hh>

namespace vull {

Mat4f Scene::get_transform_matrix(EntityId entity) {
    const auto &transform = m_world.get_component<Transform>(entity);
    if (transform.parent() == ~vull::EntityId(0)) {
        // Root node.
        return transform.matrix();
    }
    const auto parent_matrix = get_transform_matrix(transform.parent());
    return parent_matrix * transform.matrix();
}

void Scene::load(StringView scene_name) {
    // Register default components. Note that the order currently matters.
    m_world.register_component<Transform>();
    m_world.register_component<Mesh>();
    m_world.register_component<Material>();
    m_world.register_component<BoundingBox>();
    m_world.register_component<BoundingSphere>();

    // Load world.
    VULL_EXPECT(m_world.deserialise(*vpak::open(scene_name))); // TODO
}

} // namespace vull
