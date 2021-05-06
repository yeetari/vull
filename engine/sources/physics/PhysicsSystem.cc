#include <vull/physics/PhysicsSystem.hh>

#include <vull/core/Entity.hh>
#include <vull/core/Transform.hh>
#include <vull/core/World.hh>
#include <vull/physics/Collider.hh>
#include <vull/physics/RigidBody.hh>
#include <vull/physics/shape/Shape.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Vector.hh>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/matrix.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/vector_relational.hpp>

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
    const auto matrix = transform.matrix();
    return matrix * glm::vec4(shape.support_point(glm::vec4(dir, 1.0f) * matrix), 1.0f);
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

void PhysicsSystem::update(World *world, float dt) {
    // Integrate.
    for (auto [entity, body] : world->view<RigidBody>()) {
        auto *transform = entity.get<Transform>();
        ASSERT(transform != nullptr);

        // Apply gravity.
        body->apply_central_force(body->m_mass * glm::vec3(0.0f, -9.81f, 0.0f));

        // Integrate linear velocity and then position.
        glm::vec3 acceleration = body->m_force * body->m_inv_mass;
        body->m_linear_velocity += acceleration * dt;
        body->m_linear_velocity *= glm::pow(0.995f, dt);
        transform->position() += body->m_linear_velocity * dt;

        // Integrate angular velocity and then orientation.
        auto mat_orientation = glm::mat3_cast(transform->orientation());
        body->m_inertia_tensor_world = mat_orientation * body->m_inertia_tensor * glm::transpose(mat_orientation);
        body->m_angular_velocity += body->m_inertia_tensor_world * body->m_torque * dt;
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

    for (auto &contact : contacts) {
        ASSERT(contact.b0 != nullptr);
        glm::vec3 r1 = contact.point - contact.t0->position();
        glm::vec3 r2 = contact.point - contact.t1->position();
        glm::vec3 relative_velocity = contact.b0->m_linear_velocity + glm::cross(contact.b0->m_angular_velocity, r1);
        if (contact.b1 != nullptr) {
            relative_velocity -= contact.b1->m_linear_velocity + glm::cross(contact.b1->m_angular_velocity, r2);
        }

        float velocity_projection = glm::dot(relative_velocity, contact.normal);
        if (velocity_projection > 0.0f) {
            continue;
        }

        // Calculate average restitution.
        float restitution = contact.b0->m_restitution;
        if (contact.b1 != nullptr) {
            restitution += contact.b1->m_restitution;
            restitution *= 0.5f;
        }

        glm::vec3 n1 = contact.normal;
        glm::vec3 n2 = -contact.normal;
        glm::vec3 w1 = glm::cross(n1, r1);
        glm::vec3 w2 = glm::cross(n2, r2);

        float C = glm::max(0.0f, -restitution * velocity_projection - 0.9f);
        float effective_mass = contact.b0->m_inv_mass + glm::dot(w1 * contact.b0->m_inertia_tensor_world, w1);
        if (contact.b1 != nullptr) {
            effective_mass += contact.b1->m_inv_mass + glm::dot(w2 * contact.b1->m_inertia_tensor_world, w2);
        }
        float normal_impulse = (C - velocity_projection + 0.01f) / effective_mass;
        glm::vec3 impulse = contact.normal * normal_impulse;
        contact.b0->apply_impulse(impulse, r1);
        if (contact.b1 != nullptr) {
            contact.b1->apply_impulse(impulse, r2);
        }

        if (contact.penetration <= 0.0f) {
            continue;
        }

        glm::vec3 prv = contact.b0->m_linear_velocity + glm::cross(contact.b0->m_angular_velocity, r1);
        if (contact.b1 != nullptr) {
            prv -= contact.b1->m_linear_velocity + glm::cross(contact.b1->m_angular_velocity, r2);
        }

        float pvp = glm::dot(prv, contact.normal);
        if (pvp >= contact.penetration) {
            continue;
        }

        float pseudo_impulse = (contact.penetration - pvp) / effective_mass;
        glm::vec3 pseudo_impulse_vector = contact.normal * pseudo_impulse;
        contact.b0->apply_impulse(pseudo_impulse_vector, r1);
        if (contact.b1 != nullptr) {
            contact.b1->apply_impulse(-pseudo_impulse_vector, r2);
        }
    }
}
