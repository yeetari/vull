#include <vull/physics/RigidBody.hh>

RigidBody::RigidBody(float mass, const glm::vec3 &position)
    : m_mass(mass), m_inv_mass(mass != 0.0F ? 1.0F / mass : 0.0F), m_position(position) {
    // Inertia tensor for constant density sphere.
    const float mr_sqrd = 0.4F * mass * 2.5F * 2.5F;
    const glm::mat3 inertia_tensor{
        mr_sqrd, 0.0F, 0.0F, 0.0F, mr_sqrd, 0.0F, 0.0F, 0.0F, mr_sqrd,
    };
    m_inv_inertia_tensor = glm::inverse(inertia_tensor);
}

void RigidBody::apply_central_force(const glm::vec3 &force) {
    m_force += force;
}

void RigidBody::apply_torque(const glm::vec3 &torque) {
    m_torque += torque;
}
