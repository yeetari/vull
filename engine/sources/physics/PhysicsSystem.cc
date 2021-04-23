#include <vull/physics/PhysicsSystem.hh>

#include <vull/core/Entity.hh>
#include <vull/core/Transform.hh>
#include <vull/core/World.hh>
#include <vull/physics/Collider.hh>
#include <vull/physics/RigidBody.hh>
#include <vull/physics/shape/Shape.hh>
#include <vull/support/Array.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Vector.hh>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/matrix.hpp>
#include <glm/vec3.hpp>
#include <glm/vector_relational.hpp>

#include <cstdint>
#include <utility>

namespace {

struct Contact {
    RigidBody *b0;
    RigidBody *b1;
    Transform *t0;
    Transform *t1;
    glm::vec3 point;
    glm::vec3 normal;
    float penetration;
};

glm::vec3 support_transformed(const Shape &shape, const Transform &transform, const glm::vec3 &dir) {
    return shape.support_point(dir) + transform.position();
}

bool mpr_collision_test(Contact *contact, const Shape &s1, const Shape &s2, const Transform &t1, const Transform &t2) {
    const glm::vec3 &v01 = t1.position();
    const glm::vec3 &v02 = t2.position();
    glm::vec3 v0 = v02 - v01;
    if (glm::all(glm::lessThan(glm::abs(v0), glm::vec3(glm::epsilon<float>())))) {
        v0 = glm::vec3(0.00001f, 0.0f, 0.0f);
    }

    contact->normal = -v0;
    glm::vec3 v11 = support_transformed(s1, t1, v0);
    glm::vec3 v12 = support_transformed(s2, t2, contact->normal);
    glm::vec3 v1 = v12 - v11;
    if (glm::dot(v1, contact->normal) <= 0.0f) {
        return false;
    }

    contact->normal = glm::cross(v1, v0);
    if (glm::all(glm::lessThan(glm::abs(contact->normal), glm::vec3(glm::epsilon<float>())))) {
        contact->normal = glm::normalize(v1 - v0);
        contact->point = (v11 + v12) * 0.5f;
        contact->penetration = glm::dot(v12 - v11, contact->normal);
        return true;
    }

    glm::vec3 v21 = support_transformed(s1, t1, -contact->normal);
    glm::vec3 v22 = support_transformed(s2, t2, contact->normal);
    glm::vec3 v2 = v22 - v21;
    if (glm::dot(v2, contact->normal) <= 0.0f) {
        return false;
    }

    contact->normal = glm::cross(v1 - v0, v2 - v0);
    float dist = glm::dot(contact->normal, v0);
    if (dist > 0.0f) {
        std::swap(v1, v2);
        std::swap(v11, v21);
        std::swap(v12, v22);
        contact->normal = -contact->normal;
    }

    constexpr int max_iterations = 10;
    bool hit = false;
    for (int i = 0; i < max_iterations; i++) {
        glm::vec3 v31 = support_transformed(s1, t1, -contact->normal);
        glm::vec3 v32 = support_transformed(s2, t2, contact->normal);
        glm::vec3 v3 = v32 - v31;
        if (glm::dot(v3, contact->normal) <= 0.0f) {
            return false;
        }

        if (glm::dot(glm::cross(v1, v3), v0) < 0.0f) {
            v2 = v3;
            v21 = v31;
            v22 = v32;
            contact->normal = glm::cross(v1 - v0, v3 - v0);
            continue;
        }

        if (glm::dot(glm::cross(v3, v2), v0) < 0.0f) {
            v1 = v3;
            v11 = v31;
            v12 = v32;
            contact->normal = glm::cross(v3 - v0, v2 - v0);
            continue;
        }

        for (int j = 0;; j++) {
            contact->normal = glm::normalize(glm::cross(v2 - v1, v3 - v1));
            if (glm::dot(contact->normal, v1) >= 0.0f) {
                hit = true;
            }

            glm::vec3 v41 = support_transformed(s1, t1, -contact->normal);
            glm::vec3 v42 = support_transformed(s2, t2, contact->normal);
            glm::vec3 v4 = v42 - v41;
            contact->penetration = glm::dot(v4, contact->normal);

            float delta = glm::dot(v4 - v3, contact->normal);
            if (delta < 1e-4f || contact->penetration <= 0.0f || j > max_iterations) {
                if (!hit) {
                    return false;
                }
                float b0 = glm::dot(glm::cross(v1, v2), v3);
                float b1 = glm::dot(glm::cross(v3, v2), v0);
                float b2 = glm::dot(glm::cross(v0, v1), v3);
                float b3 = glm::dot(glm::cross(v2, v1), v0);
                float sum = b0 + b1 + b2 + b3;
                if (sum <= 0.0f) {
                    b0 = 0;
                    b1 = glm::dot(glm::cross(v2, v3), contact->normal);
                    b2 = glm::dot(glm::cross(v3, v1), contact->normal);
                    b3 = glm::dot(glm::cross(v1, v2), contact->normal);
                    sum = b1 + b2 + b3;
                }
                float inv = 1.0f / sum;
                contact->point = v01 * b0;
                contact->point += v11 * b1;
                contact->point += v21 * b2;
                contact->point += v31 * b3;
                contact->point += v02 * b0;
                contact->point += v12 * b1;
                contact->point += v22 * b2;
                contact->point += v32 * b3;
                contact->point *= inv * 0.5f;
                return true;
            }

            glm::vec3 tmp1 = glm::cross(v4, v0);
            float tmp2 = glm::dot(tmp1, v1);
            if (tmp2 >= 0.0f) {
                tmp2 = glm::dot(tmp1, v2);
                if (tmp2 >= 0.0f) {
                    v1 = v4;
                    v11 = v41;
                    v12 = v42;
                } else {
                    v3 = v4;
                    v31 = v41;
                    v32 = v42;
                }
            } else {
                tmp2 = glm::dot(tmp1, v3);
                if (tmp2 >= 0.0f) {
                    v2 = v4;
                    v21 = v41;
                    v22 = v42;
                } else {
                    v1 = v4;
                    v11 = v41;
                    v12 = v42;
                }
            }
        }
    }
    return false;
}

} // namespace

glm::vec3 PhysicsSystem::calculate_contact_velocity(const RigidBody *body, const glm::mat3 &contact_to_world,
                                                    const glm::vec3 &contact_position, float dt) {
    glm::vec3 body_velocity = glm::cross(body->m_angular_velocity, contact_position) + body->m_linear_velocity;
    glm::vec3 acceleration_velocity = body->m_acceleration * dt;
    acceleration_velocity = glm::transpose(contact_to_world) * acceleration_velocity;
    acceleration_velocity.x = 0.0f;
    return (glm::transpose(contact_to_world) * body_velocity) + acceleration_velocity;
}

glm::mat3 PhysicsSystem::calculate_inertia_tensor(const RigidBody *body, const Transform *transform) {
    auto mat_orientation = glm::mat3_cast(transform->orientation());
    return mat_orientation * body->m_inv_inertia_tensor * mat_orientation;
}

float PhysicsSystem::calculate_angular_inertia(const RigidBody *body, const Transform *transform,
                                               const glm::vec3 &contact_position, const glm::vec3 &contact_normal) {
    auto inv_inertia_tensor_world = calculate_inertia_tensor(body, transform);
    glm::vec3 angular_inertia_world = glm::cross(contact_position, contact_normal);
    angular_inertia_world = inv_inertia_tensor_world * angular_inertia_world;
    angular_inertia_world = glm::cross(angular_inertia_world, contact_position);
    return glm::dot(angular_inertia_world, contact_normal);
}

void PhysicsSystem::update(World *world, float dt) {
    // Integrate.
    for (auto [entity, body] : world->view<RigidBody>()) {
        auto *transform = entity.get<Transform>();
        ASSERT(transform != nullptr);

        // Apply gravity.
        body->apply_central_force(body->m_mass * glm::vec3(0.0f, -9.81f, 0.0f));

        // Integrate linear velocity and then position.
        body->m_acceleration = body->m_force * body->m_inv_mass;
        body->m_linear_velocity += body->m_acceleration * dt;
        body->m_linear_velocity *= glm::pow(0.995f, dt);
        transform->position() += body->m_linear_velocity * dt;

        // Integrate angular velocity and then orientation.
        auto mat_orientation = glm::mat3_cast(transform->orientation());
        auto inv_inertia_tensor_world = mat_orientation * body->m_inv_inertia_tensor * glm::transpose(mat_orientation);
        body->m_angular_velocity += inv_inertia_tensor_world * body->m_torque;
        body->m_angular_velocity *= glm::pow(0.995f, dt);
        transform->orientation() += glm::quat(0.0f, body->m_angular_velocity) * transform->orientation() * 0.5f * dt;
        transform->orientation() = glm::normalize(transform->orientation());

        // Clear force and torque.
        body->m_force = glm::vec3(0.0f);
        body->m_torque = glm::vec3(0.0f);
    }

    // Test collisions and store results in a list of contacts.
    Vector<Contact> contacts;
    for (auto [e0, c0] : world->view<Collider>()) {
        for (auto [e1, c1] : world->view<Collider>()) {
            // Don't check for collisions against self!
            // TODO: Remove RigidBody check once all physics objects are required to have rigid bodies.
            if (e0 == e1 || !e0.has<RigidBody>()) {
                continue;
            }
            auto *t0 = e0.get<Transform>();
            auto *t1 = e1.get<Transform>();
            ASSERT(t0 != nullptr);
            ASSERT(t1 != nullptr);
            Contact contact{
                .b0 = e0.get<RigidBody>(),
                .b1 = e1.get<RigidBody>(),
                .t0 = t0,
                .t1 = t1,
            };
            bool collided = mpr_collision_test(&contact, c0->shape(), c1->shape(), *t0, *t1);
            if (collided) {
                contacts.push(contact);
            }
        }
    }

    // Resolve contacts.
    for (auto &c : contacts) {
        Contact *contact = &c;

        ASSERT(contact->b0 != nullptr);
        Array<glm::vec3, 2> contact_position{};
        contact_position[0] = contact->point - contact->t0->position();
        if (contact->b1 != nullptr) {
            contact_position[1] = contact->point - contact->t1->position();
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
        angular_inertia[0] = calculate_angular_inertia(contact->b0, contact->t0, contact_position[0], contact->normal);
        linear_inertia[0] = contact->b0->m_inv_mass;
        if (contact->b1 != nullptr) {
            angular_inertia[1] =
                calculate_angular_inertia(contact->b1, contact->t1, contact_position[1], contact->normal);
            linear_inertia[1] = contact->b1->m_inv_mass;
        }

        float total_inertia = 0.0f;
        for (int i = 0; i < 2; i++) {
            total_inertia += angular_inertia[i] + linear_inertia[i];
        }

        Array<glm::mat3, 2> inv_inertia_tensors{};
        inv_inertia_tensors[0] = calculate_inertia_tensor(contact->b0, contact->t0);
        if (contact->b1 != nullptr) {
            inv_inertia_tensors[1] = calculate_inertia_tensor(contact->b1, contact->t1);
        }

        Array<RigidBody *, 2> bodies{contact->b0, contact->b1};
        Array<Transform *, 2> transforms{contact->t0, contact->t1};
        Array<glm::vec3, 2> angular_change{};
        Array<glm::vec3, 2> linear_change{};
        for (std::uint32_t i = 0; i < bodies.size(); i++) {
            auto *body = bodies[i];
            auto *transform = transforms[i];
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

            transform->position() += contact->normal * linear_correction;
            transform->orientation() += glm::quat(0.0f, angular_change[i]) * transform->orientation() * 0.5f;
            transform->orientation() = glm::normalize(transform->orientation());
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
}
