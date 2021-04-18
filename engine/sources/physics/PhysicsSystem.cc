#include <vull/physics/PhysicsSystem.hh>

#include <vull/core/Entity.hh>
#include <vull/core/Transform.hh>
#include <vull/core/World.hh>
#include <vull/physics/RigidBody.hh>
#include <vull/physics/SphereCollider.hh>
#include <vull/support/Array.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Vector.hh>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/matrix.hpp>
#include <glm/vec3.hpp>

#include <cstdint>

namespace {

struct Contact {
    RigidBody *b0;
    RigidBody *b1;
    glm::vec3 point;
    glm::vec3 normal;
    float penetration;
};

} // namespace

glm::vec3 PhysicsSystem::calculate_contact_velocity(const RigidBody *body, const glm::mat3 &contact_to_world,
                                                    const glm::vec3 &contact_position, float dt) {
    glm::vec3 body_velocity = glm::cross(body->m_angular_velocity, contact_position) + body->m_linear_velocity;
    glm::vec3 acceleration_velocity = body->m_acceleration * dt;
    acceleration_velocity = glm::transpose(contact_to_world) * acceleration_velocity;
    acceleration_velocity.x = 0.0f;
    return (glm::transpose(contact_to_world) * body_velocity) + acceleration_velocity;
}

glm::mat3 PhysicsSystem::calculate_inertia_tensor(const RigidBody *body) {
    auto mat_orientation = glm::mat3_cast(body->m_orientation);
    return mat_orientation * body->m_inv_inertia_tensor * mat_orientation;
}

float PhysicsSystem::calculate_angular_inertia(const RigidBody *body, const glm::vec3 &contact_position,
                                               const glm::vec3 &contact_normal) {
    auto inv_inertia_tensor_world = calculate_inertia_tensor(body);
    glm::vec3 angular_inertia_world = glm::cross(contact_position, contact_normal);
    angular_inertia_world = inv_inertia_tensor_world * angular_inertia_world;
    angular_inertia_world = glm::cross(angular_inertia_world, contact_position);
    return glm::dot(angular_inertia_world, contact_normal);
}

void PhysicsSystem::update(World *world, float dt) {
    // Integrate.
    for (auto [entity, body] : world->view<RigidBody>()) {
        // Apply gravity.
        body->apply_central_force(body->m_mass * glm::vec3(0.0f, -9.81f, 0.0f));

        // Integrate linear velocity and then position.
        body->m_acceleration = body->m_force * body->m_inv_mass;
        body->m_linear_velocity += body->m_acceleration * dt;
        body->m_linear_velocity *= glm::pow(0.995f, dt);
        body->m_position += body->m_linear_velocity * dt;

        // Integrate angular velocity and then orientation.
        auto mat_orientation = glm::mat3_cast(body->m_orientation);
        auto inv_inertia_tensor_world = mat_orientation * body->m_inv_inertia_tensor * glm::transpose(mat_orientation);
        body->m_angular_velocity += inv_inertia_tensor_world * body->m_torque;
        body->m_angular_velocity *= glm::pow(0.995f, dt);
        body->m_orientation += glm::quat(0.0f, body->m_angular_velocity) * body->m_orientation * 0.5f * dt;
        body->m_orientation = glm::normalize(body->m_orientation);

        // Clear force and torque.
        body->m_force = glm::vec3(0.0f);
        body->m_torque = glm::vec3(0.0f);
    }

    // Test collisions and store results in a list of contacts.
    Vector<Contact> contacts;
    for (auto [e0, b0, c0] : world->view<RigidBody, SphereCollider>()) {
        for (auto [e1, c1] : world->view<SphereCollider>()) {
            // Don't check for collisions against self!
            if (e0 == e1) {
                continue;
            }
            auto *b1 = e1.get<RigidBody>();
            auto *t1 = e1.get<Transform>();
            ASSERT(t1 != nullptr);

            // Check collision.
            const auto diff = b0->m_position - glm::vec3(t1->matrix()[3]);
            const auto dist = glm::length(diff);
            if (dist <= c0->m_radius + c1->m_radius) {
                auto &contact = contacts.emplace();
                contact.b0 = b0;
                contact.b1 = b1;
                contact.point = b0->m_position + diff * 0.5f;
                contact.normal = diff / dist;
                contact.penetration = c0->m_radius + c1->m_radius - dist;
            }
        }
    }

    // Resolve contacts.
    for (auto &c : contacts) {
        Contact *contact = &c;

        ASSERT(contact->b0 != nullptr);
        Array<glm::vec3, 2> contact_position{};
        contact_position[0] = contact->point - contact->b0->m_position;
        if (contact->b1 != nullptr) {
            contact_position[1] = contact->point - contact->b1->m_position;
        }

        Array<glm::vec3, 2> contact_tangent{};
        if (glm::abs(contact->normal.x) > glm::abs(contact->normal.y)) {
            const float s =
                1.0f / glm::sqrt(contact->normal.z * contact->normal.z + contact->normal.x * contact->normal.x);
            contact_tangent[0].x = contact->normal.z * s;
            contact_tangent[0].z = -contact->normal.x * s;
            contact_tangent[1].x = contact->normal.y * contact_tangent[0].x;
            contact_tangent[1].y = contact->normal.z * contact_tangent[0].x - contact->normal.x * contact_tangent[0].z;
            contact_tangent[1].z = -contact->normal.y * contact_tangent[0].x;
        } else {
            const float s =
                1.0f / glm::sqrt(contact->normal.z * contact->normal.z + contact->normal.y * contact->normal.y);
            contact_tangent[0].y = -contact->normal.z * s;
            contact_tangent[0].z = contact->normal.y * s;
            contact_tangent[1].x = contact->normal.y * contact_tangent[0].z - contact->normal.z * contact_tangent[0].y;
            contact_tangent[1].y = -contact->normal.x * contact_tangent[0].z;
            contact_tangent[1].z = contact->normal.x * contact_tangent[0].y;
        }
        glm::mat3 contact_to_world(contact->normal.x, contact_tangent[0].x, contact_tangent[1].x, contact->normal.y,
                                   contact_tangent[0].y, contact_tangent[1].y, contact->normal.z, contact_tangent[0].z,
                                   contact_tangent[1].z);
        contact_to_world = glm::transpose(contact_to_world);
        glm::vec3 contact_velocity = calculate_contact_velocity(contact->b0, contact_to_world, contact_position[0], dt);
        if (contact->b1 != nullptr) {
            contact_velocity -= calculate_contact_velocity(contact->b1, contact_to_world, contact_position[1], dt);
        }

        float velocity_from_acceleration = glm::dot(contact->b0->m_acceleration * dt, contact->normal);
        if (contact->b1 != nullptr) {
            velocity_from_acceleration -= glm::dot(contact->b1->m_acceleration * dt, contact->normal);
        }

        constexpr float velocity_limit = 0.25f;
        const float restitution =
            glm::abs(contact_velocity.x) < velocity_limit
                ? 0.0f
                : contact->b0->m_restitution - (contact->b1 != nullptr ? contact->b1->m_restitution : 0.0f);
        const float desired_delta_velocity =
            -contact_velocity.x - restitution * (contact_velocity.x - velocity_from_acceleration);

        Array<float, 2> angular_inertia{};
        Array<float, 2> linear_inertia{};
        angular_inertia[0] = calculate_angular_inertia(contact->b0, contact_position[0], contact->normal);
        linear_inertia[0] = contact->b0->m_inv_mass;
        if (contact->b1 != nullptr) {
            angular_inertia[1] = calculate_angular_inertia(contact->b1, contact_position[1], contact->normal);
            linear_inertia[1] = contact->b1->m_inv_mass;
        }

        float total_inertia = 0.0f;
        for (int i = 0; i < 2; i++) {
            total_inertia += angular_inertia[i] + linear_inertia[i];
        }

        Array<glm::mat3, 2> inv_inertia_tensors{};
        inv_inertia_tensors[0] = calculate_inertia_tensor(contact->b0);
        if (contact->b1 != nullptr) {
            inv_inertia_tensors[1] = calculate_inertia_tensor(contact->b1);
        }

        Array<RigidBody *, 2> bodies{contact->b0, contact->b1};
        Array<glm::vec3, 2> angular_change{};
        Array<glm::vec3, 2> linear_change{};
        for (std::uint32_t i = 0; i < bodies.size(); i++) {
            auto *body = bodies[i];
            if (body == nullptr) {
                continue;
            }
            const float sign = i == 0 ? 1.0f : -1.0f;
            float angular_correction = sign * contact->penetration * (angular_inertia[i] / total_inertia);
            float linear_correction = sign * contact->penetration * (linear_inertia[i] / total_inertia);
            glm::vec3 projection = contact_position[i];
            projection += contact->normal * -glm::dot(contact_position[i], contact->normal);

            constexpr float angular_limit = 0.2f;
            const float max_magnitude = angular_limit * glm::length(projection);
            const float total_move = angular_correction + linear_correction;
            if (angular_correction < -max_magnitude) {
                angular_correction = -max_magnitude;
                linear_correction = total_move - angular_correction;
            } else if (angular_correction > max_magnitude) {
                angular_correction = max_magnitude;
                linear_correction = total_move - angular_correction;
            }

            linear_change[i] = contact->normal * linear_correction;
            if (angular_correction != 0) {
                glm::vec3 target_angular_direction = glm::cross(contact_position[i], contact->normal);
                angular_change[i] =
                    inv_inertia_tensors[i] * target_angular_direction * (angular_correction / angular_inertia[i]);
            }

            body->m_position += contact->normal * linear_correction;
            body->m_orientation += glm::quat(0.0f, angular_change[i]) * body->m_orientation * 0.5f;
            body->m_orientation = glm::normalize(body->m_orientation);
        }

        float delta_velocity = 0.0f;
        {
            glm::vec3 delta_velocity_world = glm::cross(contact_position[0], contact->normal);
            delta_velocity_world = inv_inertia_tensors[0] * delta_velocity_world;
            delta_velocity_world = glm::cross(delta_velocity_world, contact_position[0]);
            delta_velocity += glm::dot(delta_velocity_world, contact->normal);
            delta_velocity += contact->b0->m_inv_mass;
        }
        if (contact->b1 != nullptr) {
            glm::vec3 delta_velocity_world = glm::cross(contact_position[1], contact->normal);
            delta_velocity_world = inv_inertia_tensors[1] * delta_velocity_world;
            delta_velocity_world = glm::cross(delta_velocity_world, contact_position[1]);
            delta_velocity += glm::dot(delta_velocity_world, contact->normal);
            delta_velocity += contact->b1->m_inv_mass;
        }

        glm::vec3 impulse_contact(desired_delta_velocity / delta_velocity, 0.0f, 0.0f);
        glm::vec3 impulse = contact_to_world * impulse_contact;
        {
            glm::vec3 impulsive_torque = glm::cross(contact_position[0], impulse);
            contact->b0->m_linear_velocity += impulse * contact->b0->m_inv_mass;
            contact->b0->m_angular_velocity += inv_inertia_tensors[0] * impulsive_torque;
        }
        if (contact->b1 != nullptr) {
            glm::vec3 impulsive_torque = glm::cross(impulse, contact_position[1]);
            contact->b1->m_linear_velocity += impulse * -contact->b1->m_inv_mass;
            contact->b1->m_angular_velocity += inv_inertia_tensors[1] * impulsive_torque;
        }
    }

    // Update transform matrices.
    for (auto [entity, body, transform] : world->view<RigidBody, Transform>()) {
        transform->matrix() = glm::translate(glm::mat4(1.0f), body->m_position) * glm::mat4_cast(body->m_orientation);
    }
}
