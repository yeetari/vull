#pragma once

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

struct PhysicsSystem;
class Shape;

class RigidBody {
    friend PhysicsSystem;

private:
    const float m_mass;
    const float m_inv_mass;
    const float m_restitution;
    float m_linear_damping{0.005f};
    float m_angular_damping{0.005f};
    glm::mat3 m_inertia_tensor{0.0f};
    glm::mat3 m_inertia_tensor_world{0.0f};
    glm::vec3 m_force{0.0f};
    glm::vec3 m_torque{0.0f};
    glm::vec3 m_linear_velocity{0.0f};
    glm::vec3 m_angular_velocity{0.0f};
    glm::vec3 m_pseudo_linear_velocity{0.0f};
    glm::vec3 m_pseudo_angular_velocity{0.0f};

public:
    RigidBody(const Shape &shape, float mass, float restitution);

    void apply_central_force(const glm::vec3 &force);
    void apply_central_impulse(const glm::vec3 &impulse);
    void apply_force(const glm::vec3 &force, const glm::vec3 &point);
    void apply_impulse(const glm::vec3 &impulse, const glm::vec3 &point);
    void apply_pseudo_impulse(const glm::vec3 &impulse, const glm::vec3 &point);
    void apply_torque(const glm::vec3 &torque);
    glm::vec3 velocity_at_point(const glm::vec3 &point) const;

    void set_linear_damping(float linear_damping) { m_linear_damping = linear_damping; }
    void set_angular_damping(float angular_damping) { m_angular_damping = angular_damping; }

    float mass() const { return m_mass; }
    float inv_mass() const { return m_inv_mass; }
    const glm::mat3 &inertia_tensor() const { return m_inertia_tensor; }
    const glm::vec3 &linear_velocity() const { return m_linear_velocity; }
    const glm::vec3 &angular_velocity() const { return m_angular_velocity; }
};

constexpr float operator""_kg(long double mass) {
    return static_cast<float>(mass);
}

constexpr float operator""_t(long double mass) {
    return static_cast<float>(mass) * 1000.0f;
}
