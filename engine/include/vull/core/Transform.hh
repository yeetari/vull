#pragma once

#include <vull/core/BuiltinComponents.hh>
#include <vull/ecs/Component.hh>
#include <vull/ecs/EntityId.hh>
#include <vull/maths/Mat.hh>
#include <vull/maths/Quat.hh>
#include <vull/maths/Vec.hh>

namespace vull {

class Transform {
    VULL_DECLARE_COMPONENT(BuiltinComponents::Transform);

private:
    EntityId m_parent;
    Vec3f m_position;
    Quatf m_rotation;
    Vec3f m_scale;

public:
    Transform(EntityId parent, const Vec3f &position = {}, const Quatf &rotation = {}, const Vec3f &scale = {1.0f})
        : m_parent(parent), m_position(position), m_rotation(rotation), m_scale(scale) {}

    Mat4f matrix() const;

    EntityId parent() const { return m_parent; }
    const Vec3f &position() const { return m_position; }
    const Vec3f &scale() const { return m_scale; }
};

} // namespace vull
