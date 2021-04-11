#pragma once

#include <glm/gtc/quaternion.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

class PhysicsSystem;

class RigidBody {
    friend PhysicsSystem;

private:
    const float m_mass;
    const float m_inv_mass;
    glm::mat3 m_inv_inertia_tensor{0.0f};
    glm::vec3 m_force{0.0f};
    glm::vec3 m_torque{0.0f};
    glm::vec3 m_linear_velocity{0.0f};
    glm::vec3 m_angular_velocity{0.0f};

    glm::vec3 m_acceleration{0.0f};

    glm::vec3 m_position;
    glm::quat m_orientation{0.0f, 0.0f, 0.0f, 0.0f};

public:
    RigidBody(float mass, const glm::vec3 &position);

    void apply_central_force(const glm::vec3 &force);
    void apply_torque(const glm::vec3 &torque);
};
