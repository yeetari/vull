#pragma once

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

class PhysicsSystem;
class Shape;

class RigidBody {
    friend PhysicsSystem;

private:
    const float m_mass;
    const float m_inv_mass;
    const float m_restitution;
    glm::mat3 m_inertia_tensor{0.0f};
    glm::mat3 m_inertia_tensor_world{0.0f};
    glm::vec3 m_force{0.0f};
    glm::vec3 m_torque{0.0f};
    glm::vec3 m_linear_velocity{0.0f};
    glm::vec3 m_angular_velocity{0.0f};

public:
    RigidBody(const Shape &shape, float mass, float restitution);

    void apply_central_force(const glm::vec3 &force);
    void apply_central_impulse(const glm::vec3 &impulse);
    void apply_impulse(const glm::vec3 &impulse, const glm::vec3 &point);
    void apply_torque(const glm::vec3 &torque);

    const glm::vec3 &linear_velocity() const { return m_linear_velocity; }
    const glm::vec3 &angular_velocity() const { return m_angular_velocity; }
};
