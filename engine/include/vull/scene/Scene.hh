#pragma once

#include <vull/ecs/EntityId.hh>
#include <vull/ecs/World.hh>
#include <vull/maths/Mat.hh>
#include <vull/support/StringView.hh>

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
