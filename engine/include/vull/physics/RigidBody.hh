#pragma once

#include <glm/vec3.hpp>

class PhysicsSystem;

class RigidBody {
    friend PhysicsSystem;

private:
    const float m_mass;
    const float m_inv_mass;
    glm::vec3 m_force{0.0F};
    glm::vec3 m_velocity{0.0F};

public:
    explicit RigidBody(float mass) : m_mass(mass), m_inv_mass(mass != 0.0F ? 1.0F / mass : 0.0F) {}
};
