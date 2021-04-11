#include <vull/physics/RigidBody.hh>

#include <glm/matrix.hpp>

RigidBody::RigidBody(float mass, const glm::vec3 &position)
    : m_mass(mass), m_inv_mass(mass != 0.0f ? 1.0f / mass : 0.0f), m_position(position) {
    // Inertia tensor for constant density sphere.
    const float mr_sqrd = 0.4f * mass * 2.5f * 2.5f;
    const glm::mat3 inertia_tensor{
        mr_sqrd, 0.0f, 0.0f, 0.0f, mr_sqrd, 0.0f, 0.0f, 0.0f, mr_sqrd,
    };
    m_inv_inertia_tensor = glm::inverse(inertia_tensor);
}

void RigidBody::apply_central_force(const glm::vec3 &force) {
    m_force += force;
}

void RigidBody::apply_torque(const glm::vec3 &torque) {
    m_torque += torque;
}
