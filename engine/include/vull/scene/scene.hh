#pragma once

#include <vull/ecs/entity_id.hh>
#include <vull/ecs/world.hh>
#include <vull/maths/mat.hh>
#include <vull/support/string_view.hh>

namespace vull {

class Scene {
    World m_world;

public:
    Scene() = default;
    Scene(const Scene &) = delete;
    Scene(Scene &&) = delete;
    ~Scene() = default;

    Scene &operator=(const Scene &) = delete;
    Scene &operator=(Scene &&) = delete;

    Mat4f get_transform_matrix(EntityId entity);
    void load(StringView scene_name);

    World &world() { return m_world; }
};

} // namespace vull
