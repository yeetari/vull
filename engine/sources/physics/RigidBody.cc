#include <vull/physics/RigidBody.hh>

#include <glm/geometric.hpp>
#include <glm/matrix.hpp>

RigidBody::RigidBody(float mass, float restitution)
    : m_mass(mass), m_inv_mass(mass != 0.0f ? 1.0f / mass : 0.0f), m_restitution(restitution) {
    // Inertia tensor for constant density sphere.
    const float mr_sqrd = 0.4f * mass * 2.5f * 2.5f;
    const glm::mat3 inertia_tensor{
        mr_sqrd, 0.0f, 0.0f, 0.0f, mr_sqrd, 0.0f, 0.0f, 0.0f, mr_sqrd,
    };
    m_inertia_tensor = glm::inverse(inertia_tensor);
}

void RigidBody::apply_central_force(const glm::vec3 &force) {
    m_force += force;
}

void RigidBody::apply_central_impulse(const glm::vec3 &impulse) {
    apply_impulse(impulse, glm::vec3(0.0f));
}

void RigidBody::apply_impulse(const glm::vec3 &impulse, const glm::vec3 &point) {
    m_linear_velocity += impulse * m_inv_mass;
    m_angular_velocity += glm::cross(point, impulse) * m_inertia_tensor_world;
}

void RigidBody::apply_torque(const glm::vec3 &torque) {
    m_torque += torque;
}
