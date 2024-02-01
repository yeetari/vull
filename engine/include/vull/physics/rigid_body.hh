#pragma once

#include <vull/core/builtin_components.hh>
#include <vull/ecs/component.hh>
#include <vull/maths/mat.hh>
#include <vull/maths/vec.hh>

namespace vull {

class PhysicsEngine;
struct Shape;

class RigidBody {
    friend PhysicsEngine;
    VULL_DECLARE_COMPONENT(BuiltinComponents::RigidBody);

private:
    Mat3f m_inertia_tensor;
    Mat3f m_inertia_tensor_world;
    Vec3f m_linear_velocity;
    Vec3f m_angular_velocity;
    Vec3f m_pseudo_linear_velocity;
    Vec3f m_pseudo_angular_velocity;
    Vec3f m_force;
    Vec3f m_torque;
    float m_inv_mass;
    bool m_ignore_rotation{false};

public:
    RigidBody(float mass) : m_inv_mass(1.0f / mass) {}

    void apply_central_force(const Vec3f &force);
    void apply_force(const Vec3f &force, const Vec3f &point);
    void apply_impulse(const Vec3f &impulse, const Vec3f &point);
    void apply_psuedo_impulse(const Vec3f &impulse, const Vec3f &point);
    void set_ignore_rotation(bool ignore_rotation);
    void set_shape(const Shape &shape);
    Vec3f velocity_at_point(const Vec3f &point) const;

    Vec3f linear_velocity() const { return m_linear_velocity; }
};

} // namespace vull
