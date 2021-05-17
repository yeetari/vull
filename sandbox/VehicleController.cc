#include "VehicleController.hh"

#include <vull/core/Entity.hh>
#include <vull/core/World.hh>
#include <vull/io/Window.hh>
#include <vull/physics/RigidBody.hh>
#include <vull/physics/Vehicle.hh>
#include <vull/support/Vector.hh>

#include <GLFW/glfw3.h>
#include <glm/common.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/epsilon.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>

void VehicleControllerSystem::update(World *world, float dt) {
    for (auto [player, chassis, controller, vehicle] : world->view<RigidBody, VehicleController, Vehicle>()) {
        constexpr float speed = 37500.0f;
        float engine_force = 0.0f;
        if (glfwGetKey(*m_window, GLFW_KEY_UP) == GLFW_PRESS) {
            engine_force += speed;
        }
        if (glfwGetKey(*m_window, GLFW_KEY_DOWN) == GLFW_PRESS) {
            engine_force -= speed;
        }
        if (glfwGetKey(*m_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
            engine_force *= 2.0f;
        }

        // Apply engine force to wheels, assuming all axles are powered.
        for (auto &axle : vehicle->axles()) {
            for (auto &wheel : axle.wheels()) {
                wheel.set_engine_force(engine_force);
            }
        }

        // Flip force.
        if (glfwGetKey(*m_window, GLFW_KEY_N) == GLFW_PRESS) {
            chassis->apply_force(glm::vec3(0.0f, 50000.0f, 0.0f), glm::vec3(0.0f, 0.0f, -5.0f));
        }

        constexpr float steer_speed = 0.4f;
        if (glfwGetKey(*m_window, GLFW_KEY_LEFT) == GLFW_PRESS) {
            m_steering += dt * steer_speed;
        } else if (glfwGetKey(*m_window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
            m_steering -= dt * steer_speed;
        } else {
            m_steering = glm::mix(m_steering, 0.0f, dt * 5.0f);
        }
        m_steering = glm::clamp(m_steering, glm::radians(-15.0f), glm::radians(15.0f));

        // Ackermann steering.
        auto &front_axle = vehicle->axles()[0];
        auto &rear_axle = vehicle->axles()[1];
        auto &fl_wheel = front_axle.wheels()[0];
        auto &fr_wheel = front_axle.wheels()[1];
        float axle_separation = front_axle.z_offset() - rear_axle.z_offset();
        float wheel_separation = fr_wheel.x_offset() - fl_wheel.x_offset();
        if (!glm::epsilonEqual(m_steering, 0.0f, glm::epsilon<float>())) {
            float turning_circle_radius = axle_separation / glm::tan(m_steering);
            fl_wheel.set_steering(axle_separation / (turning_circle_radius + (wheel_separation / 2.0f)));
            fr_wheel.set_steering(axle_separation / (turning_circle_radius - (wheel_separation / 2.0f)));
        } else {
            fl_wheel.set_steering(0.0f);
            fr_wheel.set_steering(0.0f);
        }
    }
}
