#include <vull/physics/PhysicsSystem.hh>

#include <vull/core/Transform.hh>
#include <vull/core/World.hh>
#include <vull/physics/RigidBody.hh>
#include <vull/physics/SphereCollider.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Vector.hh>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace {

struct Contact {
    RigidBody *b0;
    RigidBody *b1;
    Transform *t0;
    Transform *t1;
    glm::vec3 normal;
    float penetration;
    float separating_velocity;
};

} // namespace

void PhysicsSystem::update(World *world, float dt) {
    // Test collisions and store results in a list of contacts.
    Vector<Contact> contacts;
    for (auto e0 : world->view<RigidBody, SphereCollider>()) {
        auto *b0 = e0.get<RigidBody>();
        auto *c0 = e0.get<SphereCollider>();
        auto *t0 = e0.get<Transform>();
        ASSERT(t0 != nullptr);
        for (auto e1 : world->view<SphereCollider>()) {
            // Don't check for collisions against self!
            if (e0 == e1) {
                continue;
            }
            auto *b1 = e1.get<RigidBody>();
            auto *c1 = e1.get<SphereCollider>();
            auto *t1 = e1.get<Transform>();
            ASSERT(t1 != nullptr);

            // Check collision.
            const auto diff = t0->position() - t1->position();
            const auto dist = glm::length(diff);
            if (dist <= c0->m_radius + c1->m_radius) {
                auto &contact = contacts.emplace();
                contact.b0 = b0;
                contact.b1 = b1;
                contact.t0 = t0;
                contact.t1 = t1;
                contact.normal = diff / dist;
                contact.penetration = c0->m_radius + c1->m_radius - dist;
            }
        }
    }

    // Resolve contacts.
    while (true) {
        Contact *contact = nullptr;
        float max_separating_velocity = 0.0F;
        for (auto &potential_contact : contacts) {
            glm::vec3 relative_velocity = potential_contact.b0->m_velocity;
            if (potential_contact.b1 != nullptr) {
                relative_velocity -= potential_contact.b1->m_velocity;
            }
            potential_contact.separating_velocity = glm::dot(relative_velocity, potential_contact.normal);
            if (potential_contact.separating_velocity < max_separating_velocity &&
                (potential_contact.separating_velocity < 0.0F || potential_contact.penetration > 0.0F)) {
                max_separating_velocity = potential_contact.separating_velocity;
                contact = &potential_contact;
            }
        }

        if (contact == nullptr) {
            break;
        }

        float total_inv_mass = contact->b0->m_inv_mass;
        if (contact->b1 != nullptr) {
            total_inv_mass += contact->b1->m_inv_mass;
        }
        ASSERT(total_inv_mass > 0.0F);

        // Resolve velocity by applying impulse.
        if (contact->separating_velocity <= 0.0F) {
            const float restitution = 1.0F;
            const float new_separating_velocity = -contact->separating_velocity * restitution;
            const float delta_velocity = new_separating_velocity - contact->separating_velocity;
            glm::vec3 impulse = contact->normal * (delta_velocity / total_inv_mass);
            contact->b0->m_velocity += impulse * contact->b0->m_inv_mass;
            if (contact->b1 != nullptr) {
                // Need to negate b1's inverse mass here since normal, and therefore impulse, is from b0's perspective.
                contact->b1->m_velocity += impulse * -contact->b1->m_inv_mass;
            }
        }

        // Resolve penetration by correcting both bodies' position to make sure there distance is 0.
        if (contact->penetration > 0.0F) {
            glm::vec3 correction = contact->normal * (-contact->penetration / total_inv_mass);
            glm::vec3 t0_correction = correction * contact->b0->m_inv_mass;
            contact->t0->position() += glm::vec4(t0_correction, 0.0F);
            glm::vec3 t1_correction(0.0F);
            if (contact->b1 != nullptr) {
                t1_correction = correction * -contact->b1->m_inv_mass;
                contact->t1->position() += glm::vec4(t1_correction, 0.0F);
            }
            for (auto &other_contact : contacts) {
                if (contact->b0 == other_contact.b0) {
                    other_contact.penetration -= glm::dot(t0_correction, other_contact.normal);
                } else if (contact->b1 == other_contact.b0) {
                    ASSERT(contact->b1 != nullptr);
                    other_contact.penetration -= glm::dot(t1_correction, other_contact.normal);
                }
                if (other_contact.b1 != nullptr) {
                    if (contact->b0 == other_contact.b1) {
                        other_contact.penetration += glm::dot(t0_correction, other_contact.normal);
                    } else if (contact->b1 == other_contact.b1) {
                        ASSERT(contact->b1 != nullptr);
                        other_contact.penetration += glm::dot(t1_correction, other_contact.normal);
                    }
                }
            }
        }
    }

    for (auto entity : world->view<RigidBody>()) {
        auto *body = entity.get<RigidBody>();
        auto *transform = entity.get<Transform>();
        ASSERT(transform != nullptr);

        // Apply gravity.
        // W = mg
        body->m_force += body->m_mass * glm::vec3(0.0F, -9.81F, 0.0F);

        // Integrate velocity and then position.
        // F = ma
        // ∴ a = F/m
        // ≡ a = F * 1/m
        // a = Δv/Δt
        // ∴ Δv = aΔt
        // v = Δp/Δt
        // ∴ Δp = vΔt
        auto &position = transform->position();
        body->m_velocity += body->m_force * body->m_inv_mass * dt;
        body->m_velocity *= glm::pow(0.995F, dt);
        position += glm::vec4(body->m_velocity * dt, 0.0F);

        // Clear force.
        body->m_force = glm::vec3(0.0F);
    }
}
