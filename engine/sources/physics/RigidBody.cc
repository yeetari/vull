#include <vull/physics/RigidBody.hh>

#include <vull/physics/shape/Shape.hh>

#include <glm/geometric.hpp>
#include <glm/matrix.hpp>

RigidBody::RigidBody(const Shape &shape, float mass, float restitution)
    : m_mass(mass), m_inv_mass(mass != 0.0f ? 1.0f / mass : 0.0f), m_restitution(restitution) {
    if (mass != 0.0f) {
        m_inertia_tensor = glm::inverse(shape.inertia_tensor(mass));
    }
}

void RigidBody::apply_central_force(const glm::vec3 &force) {
    apply_force(force, glm::vec3(0.0f));
}

void RigidBody::apply_central_impulse(const glm::vec3 &impulse) {
    apply_impulse(impulse, glm::vec3(0.0f));
}

void RigidBody::apply_force(const glm::vec3 &force, const glm::vec3 &point) {
    m_force += force;
    m_torque += glm::cross(point, force);
}

void RigidBody::apply_impulse(const glm::vec3 &impulse, const glm::vec3 &point) {
    m_linear_velocity += impulse * m_inv_mass;
    m_angular_velocity += m_inertia_tensor_world * glm::cross(point, impulse);
}

void RigidBody::apply_pseudo_impulse(const glm::vec3 &impulse, const glm::vec3 &point) {
    m_pseudo_linear_velocity += impulse * m_inv_mass;
    m_pseudo_angular_velocity += m_inertia_tensor_world * glm::cross(point, impulse);
}

void RigidBody::apply_torque(const glm::vec3 &torque) {
    m_torque += torque;
}

glm::vec3 RigidBody::velocity_at_point(const glm::vec3 &point) const {
    return m_linear_velocity + glm::cross(m_angular_velocity, point);
}
