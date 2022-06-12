#include <vull/physics/RigidBody.hh>

#include <vull/maths/Mat.hh>
#include <vull/maths/Vec.hh>
#include <vull/physics/Shape.hh>

namespace vull {

void RigidBody::apply_central_force(const Vec3f &force) {
    m_force += force;
}

void RigidBody::apply_force(const Vec3f &force, const Vec3f &point) {
    m_force += force;
    m_torque += vull::cross(point, force);
}

void RigidBody::apply_impulse(const Vec3f &impulse, const Vec3f &point) {
    m_linear_velocity += impulse * m_inv_mass;
    m_angular_velocity += m_inertia_tensor_world * vull::cross(point, impulse);
}

void RigidBody::apply_psuedo_impulse(const Vec3f &impulse, const Vec3f &point) {
    m_pseudo_linear_velocity += impulse * m_inv_mass;
    m_pseudo_angular_velocity += m_inertia_tensor_world * vull::cross(point, impulse);
}

void RigidBody::set_shape(const Shape &shape) {
    m_inertia_tensor = vull::inverse(shape.inertia_tensor(1.0f / m_inv_mass));
}

Vec3f RigidBody::velocity_at_point(const Vec3f &point) const {
    return m_linear_velocity + vull::cross(m_angular_velocity, point);
}

} // namespace vull
