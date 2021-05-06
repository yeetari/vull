#include "VehicleController.hh"

#include <vull/core/Entity.hh>
#include <vull/core/Transform.hh>
#include <vull/core/World.hh>
#include <vull/io/Window.hh>
#include <vull/physics/RigidBody.hh>
#include <vull/physics/Vehicle.hh>
#include <vull/support/Vector.hh>

#include <GLFW/glfw3.h>
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>

void VehicleControllerSystem::update(World *world, float) {
    for (auto [player, chassis, controller, transform, vehicle] :
         world->view<RigidBody, VehicleController, Transform, Vehicle>()) {
        constexpr float speed = 15000.0f;
        float engine_force = 0.0f;
        if (glfwGetKey(*m_window, GLFW_KEY_UP) == GLFW_PRESS) {
            engine_force += speed;
        }
        if (glfwGetKey(*m_window, GLFW_KEY_DOWN) == GLFW_PRESS) {
            engine_force -= speed;
        }

        // Apply engine force to wheels, assuming all axles are powered.
        for (auto &axle : vehicle->axles()) {
            for (auto &wheel : axle.wheels()) {
                wheel.set_engine_force(engine_force);
            }
        }

        // Flip force.
        if (glfwGetKey(*m_window, GLFW_KEY_N) == GLFW_PRESS) {
            chassis->apply_force(glm::vec3(0.0f, 50000.0f, 0.0f),
                                 transform->orientation() * glm::vec3(0.0f, 0.0f, -5.0f));
        }

        constexpr float angle = glm::radians(5.0f);
        float steering = 0.0f;
        if (glfwGetKey(*m_window, GLFW_KEY_LEFT) == GLFW_PRESS) {
            steering = angle;
        } else if (glfwGetKey(*m_window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
            steering = -angle;
        }

        // Ackermann steering.
        auto &front_axle = vehicle->axles()[0];
        auto &rear_axle = vehicle->axles()[1];
        auto &fl_wheel = front_axle.wheels()[0];
        auto &fr_wheel = front_axle.wheels()[1];
        float axle_separation = front_axle.z_offset() - rear_axle.z_offset();
        float wheel_separation = fr_wheel.x_offset() - fl_wheel.x_offset();
        if (steering != 0.0f) {
            float turning_circle_radius = axle_separation / glm::tan(steering);
            fl_wheel.set_steering(axle_separation / (turning_circle_radius + (wheel_separation / 2.0f)));
            fr_wheel.set_steering(axle_separation / (turning_circle_radius - (wheel_separation / 2.0f)));
        } else {
            fl_wheel.set_steering(0.0f);
            fr_wheel.set_steering(0.0f);
        }
    }
}
