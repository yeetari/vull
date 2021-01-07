#include <vull/physics/PhysicsSystem.hh>

#include <vull/core/Transform.hh>
#include <vull/core/World.hh>
#include <vull/io/Window.hh>
#include <vull/physics/RigidBody.hh>
#include <vull/physics/SphereCollider.hh>
#include <vull/physics/Spring.hh>
#include <vull/physics/Vehicle.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Log.hh>

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

static float g_rot = 0.0F;

void PhysicsSystem::update(World *world, float dt) {
    for (auto e0 : world->view<SphereCollider>()) {
        auto *t0 = e0.get<Transform>();
        ASSERT(t0 != nullptr);
        const auto &p0 = t0->matrix()[3];
        for (auto e1 : world->view<SphereCollider>()) {
            if (e0 == e1) {
                continue;
            }
            auto *t1 = e1.get<Transform>();
            ASSERT(t1 != nullptr);
            const auto &p1 = t1->matrix()[3];
            const auto diff = p0 - p1;
            const auto dist = glm::length(diff);
            if (dist > 2.5F + 2.5F) {
                continue;
            }
            auto *b0 = e0.get<RigidBody>();
            if (b0 == nullptr) {
                continue;
            }
            glm::vec3 relative_velocity = b0->m_velocity;
            if (auto *b1 = e1.get<RigidBody>()) {
                relative_velocity -= b1->m_velocity;
            }
            // Normal frmo b0's perspective.
            glm::vec3 normal = diff / dist;
            const float separating_velocity = glm::dot(relative_velocity, normal);
            if (separating_velocity > 0.0F) {
                continue;
            }
            glm::vec3 acc_caused_velocity = b0->m_acceleration;
            if (auto *b1 = e1.get<RigidBody>()) {
                acc_caused_velocity -= b1->m_acceleration;
            }
            const float acc_caused_separation_velocity = glm::dot(acc_caused_velocity, normal) * dt;
            const float restitution = 1.0F;
            float new_separating_velocity = -separating_velocity * restitution;
            if (acc_caused_separation_velocity < 0.0F) {
                new_separating_velocity += restitution * acc_caused_separation_velocity;
                if (new_separating_velocity < 0) {
                    new_separating_velocity = 0;
                }
            }

            const float delta_velocity = new_separating_velocity - separating_velocity;
            float total_inv_mass = b0->m_inv_mass;
            if (auto *b1 = e1.get<RigidBody>()) {
                total_inv_mass += b1->m_inv_mass;
            }
            ASSERT(total_inv_mass > 0);

            // Apply velocity impulse.
            glm::vec3 impulse = normal * (delta_velocity / total_inv_mass);
            b0->m_velocity += impulse * b0->m_inv_mass;
            if (auto *b1 = e1.get<RigidBody>()) {
                // Need to - here since normal, and therefore impulse, is from b0's perspective.
                b1->m_velocity += impulse * -b1->m_inv_mass;
            }

            // Resolve penetration by moving colliding objects out of each other so that they're distance is 0.
            const float penetration = 2.5F + 2.5F - dist;
            if (penetration <= 0) {
                continue;
            }
            glm::vec3 correction = normal * (-penetration / total_inv_mass);
            b0->m_position += correction * b0->m_inv_mass;
            if (auto *b1 = e1.get<RigidBody>()) {
                b1->m_position += correction * b1->m_inv_mass;
            }
        }
    }

    // Vehicle handling.
    for (auto entity : world->view<RigidBody, Vehicle>()) {
        auto *chassis = entity.get<RigidBody>();
        auto *vehicle = entity.get<Vehicle>();
        auto &fl_wheel = vehicle->fl_wheel().get<Transform>()->matrix();
        auto &fr_wheel = vehicle->fr_wheel().get<Transform>()->matrix();
        auto &rl_wheel = vehicle->rl_wheel().get<Transform>()->matrix();
        auto &rr_wheel = vehicle->rr_wheel().get<Transform>()->matrix();

        // Apply engine force or braking force.
        const glm::vec3 orientation(-1.0F, 0.0F, 0.0F);
        if (glfwGetKey(*m_window, GLFW_KEY_UP) == GLFW_PRESS) {
            chassis->m_force += orientation * 250.0F;
        }
        if (glfwGetKey(*m_window, GLFW_KEY_DOWN) == GLFW_PRESS) {
            chassis->m_force -= orientation * 100.0F;
        }

        // Apply drag.
        // C_drag = 0.5 * AρC_d
        // where A = frontal area of vehicle
        //       ρ = density of fluid
        //       C_d = friction coefficient
        // ρ_air = 1.29
        // F_drag = -C_drag * v * |v|
        const float C_drag = 0.5F * 1.0F * 1.29F * 0.05F;
        chassis->m_force -= C_drag * chassis->m_velocity * glm::length(chassis->m_velocity);

        // Apply rolling resistance.
        // C_rr = C_drag * 30
        // F_rr = -vC_rr
        const float C_rr = C_drag * 30;
        chassis->m_force -= chassis->m_velocity * C_rr;

        // Update wheels.
        fl_wheel = glm::translate(glm::mat4(1.0F), chassis->m_position + glm::vec3(-8, 0, 5));
        fr_wheel = glm::translate(glm::mat4(1.0F), chassis->m_position + glm::vec3(-8, 0, -5));
        rl_wheel = glm::translate(glm::mat4(1.0F), chassis->m_position + glm::vec3(8, 0, 5));
        rr_wheel = glm::translate(glm::mat4(1.0F), chassis->m_position + glm::vec3(8, 0, -5));

        fl_wheel = glm::rotate(fl_wheel, glm::radians(90.0F), glm::vec3(1, 0, 0));
        fr_wheel = glm::rotate(fr_wheel, glm::radians(270.0F), glm::vec3(1, 0, 0));
        rl_wheel = glm::rotate(rl_wheel, glm::radians(90.0F), glm::vec3(1, 0, 0));
        rr_wheel = glm::rotate(rr_wheel, glm::radians(270.0F), glm::vec3(1, 0, 0));

        g_rot += glm::dot(chassis->m_velocity, orientation) / 500.0F;
        fl_wheel = glm::rotate(fl_wheel, g_rot, glm::vec3(0, 1, 0));
        fr_wheel = glm::rotate(fr_wheel, -g_rot, glm::vec3(0, 1, 0));
        rl_wheel = glm::rotate(rl_wheel, g_rot, glm::vec3(0, 1, 0));
        rr_wheel = glm::rotate(rr_wheel, -g_rot, glm::vec3(0, 1, 0));

        fl_wheel = glm::scale(fl_wheel, glm::vec3(4.0F));
        fr_wheel = glm::scale(fr_wheel, glm::vec3(4.0F));
        rl_wheel = glm::scale(rl_wheel, glm::vec3(4.0F));
        rr_wheel = glm::scale(rr_wheel, glm::vec3(4.0F));
    }

    for (auto entity : world->view<Spring>()) {
        auto *body = entity.get<RigidBody>();
        auto *spring = entity.get<Spring>();
        ASSERT(body != nullptr);

        glm::vec3 force = body->m_position - spring->m_point;
        float extension = glm::length(force);
        extension = glm::abs(extension - spring->m_rest_length);
        extension *= spring->m_spring_constant;

        force = glm::normalize(force);
        force *= -extension;
        body->m_force += force;
    }

    for (auto entity : world->view<RigidBody>()) {
        auto *body = entity.get<RigidBody>();

        // Apply gravity.
        // W = mg
        glm::vec3 weight = body->m_mass * glm::vec3(0.0F, -9.81F, 0.0F);
        if (!entity.has<Vehicle>()) {
            body->m_force += weight;
        }

        // Integrate velocity and then position.
        // F = ma
        // ∴ a = F/m
        // ≡ a = F * 1/m
        // a = Δv/Δt
        // ∴ Δv = aΔt
        // v = Δp/Δt
        // ∴ Δp = vΔt
        body->m_acceleration = body->m_force * body->m_inv_mass;
        body->m_velocity += body->m_force * body->m_inv_mass * dt;
        body->m_velocity *= glm::pow(0.995F, dt);
        body->m_position += body->m_velocity * dt;

        // Clear force.
        body->m_force = glm::vec3(0.0F);

        // Update transform's position.
        auto *transform = entity.get<Transform>();
        ASSERT(transform != nullptr);
        transform->matrix() = glm::translate(glm::mat4(1.0F), body->m_position);
        if (entity.has<Vehicle>()) {
            transform->matrix() = glm::scale(transform->matrix(), glm::vec3(12.0F, 1.0F, 5.0F));
        }
    }
}
