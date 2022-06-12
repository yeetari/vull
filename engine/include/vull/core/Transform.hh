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

    Vec3f forward() const;
    Vec3f right() const;
    Vec3f up() const;
    Mat4f matrix() const;

    void set_position(const Vec3f &position) { m_position = position; }
    void set_rotation(const Quatf &rotation) { m_rotation = rotation; }
    void set_scale(const Vec3f &scale) { m_scale = scale; }

    EntityId parent() const { return m_parent; }
    const Vec3f &position() const { return m_position; }
    const Quatf &rotation() const { return m_rotation; }
    const Vec3f &scale() const { return m_scale; }
};

// Transform point in local space to world space.
inline Vec3f operator*(const Transform &transform, const Vec3f &point) {
    return transform.position() + vull::rotate(transform.rotation(), point);
}

} // namespace vull
