#pragma once

#include <vull/core/System.hh>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

class RigidBody;
class Transform;
struct World;

class PhysicsSystem final : public System<PhysicsSystem> {
    glm::vec3 calculate_contact_velocity(const RigidBody *body, const glm::mat3 &contact_to_world,
                                         const glm::vec3 &contact_position, float dt);
    glm::mat3 calculate_inertia_tensor(const RigidBody *body, const Transform *transform);
    float calculate_angular_inertia(const RigidBody *body, const Transform *transform,
                                    const glm::vec3 &contact_position, const glm::vec3 &contact_normal);

public:
    void update(World *world, float dt) override;
};
