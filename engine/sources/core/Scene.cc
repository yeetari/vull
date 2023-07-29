#include <vull/core/Scene.hh>

#include <vull/core/BoundingBox.hh>
#include <vull/core/BoundingSphere.hh>
#include <vull/core/Transform.hh>
#include <vull/ecs/EntityId.hh>
#include <vull/ecs/World.hh>
#include <vull/graphics/Material.hh>
#include <vull/graphics/Mesh.hh>
#include <vull/maths/Mat.hh>
#include <vull/support/Result.hh>
#include <vull/support/StringView.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/support/Utility.hh>
#include <vull/vpak/FileSystem.hh>
#include <vull/vpak/Reader.hh>

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
