#include "PlayerController.hh"

#include <vull/core/Entity.hh>
#include <vull/core/Transform.hh>
#include <vull/core/World.hh>
#include <vull/io/Window.hh>
#include <vull/physics/RigidBody.hh>
#include <vull/renderer/Camera.hh>
#include <vull/support/Assert.hh>

#include <GLFW/glfw3.h>
#include <glm/vec3.hpp>

void PlayerControllerSystem::update(World *world, float) {
    for (auto [player, body, camera, controller] : world->view<RigidBody, Camera, PlayerController>()) {
        auto *transform = player.get<Transform>();
        ASSERT(transform != nullptr);

        glm::vec3 forward = camera->forward();
        glm::vec3 right = camera->right();
        forward.y = 0.0f;
        right.y = 0.0f;
        camera->set_position(transform->position() + glm::vec3(0.0f, 15.0f, 0.0f) - forward * 100.0f);
        float speed = 20000.0f;
        if (glfwGetKey(*m_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
            speed *= 5.0f;
        }

        if (glfwGetKey(*m_window, GLFW_KEY_H) == GLFW_PRESS) {
            body->apply_torque(glm::vec3(0.0f, 400000.0f, 0.0f));
        }
        if (glfwGetKey(*m_window, GLFW_KEY_J) == GLFW_PRESS) {
            body->apply_torque(glm::vec3(0.0f, 1000000.0f, 0.0f));
        }

        if (glfwGetKey(*m_window, GLFW_KEY_W) == GLFW_PRESS) {
            glm::vec3 target = forward * speed;
            glm::vec3 change = target - body->linear_velocity();
            body->apply_central_impulse(change);
        }
        if (glfwGetKey(*m_window, GLFW_KEY_S) == GLFW_PRESS) {
            glm::vec3 target = -forward * speed;
            glm::vec3 change = target - body->linear_velocity();
            body->apply_central_impulse(change);
        }
        if (glfwGetKey(*m_window, GLFW_KEY_A) == GLFW_PRESS) {
            glm::vec3 target = -right * speed;
            glm::vec3 change = target - body->linear_velocity();
            body->apply_central_impulse(change);
        }
        if (glfwGetKey(*m_window, GLFW_KEY_D) == GLFW_PRESS) {
            glm::vec3 target = right * speed;
            glm::vec3 change = target - body->linear_velocity();
            body->apply_central_impulse(change);
        }

        if (glfwGetKey(*m_window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            if (!m_space_pressed) {
                body->apply_central_impulse(glm::vec3(0.0f, 2000000.0f, 0.0f));
            }
            m_space_pressed = true;
        } else {
            m_space_pressed = false;
        }

        glm::vec3 change = -body->linear_velocity();
        change.y = 0.0f;
        body->apply_central_impulse(change);
    }
}
