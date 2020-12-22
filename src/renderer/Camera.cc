#include <renderer/Camera.hh>

#include <Window.hh>

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

namespace {

constexpr float MOUSE_SENSITIVITY = 0.05F;
constexpr float MOVEMENT_SPEED = 0.1F;
constexpr glm::vec3 WORLD_UP(0.0F, 0.0F, 1.0F);

} // namespace

void Camera::handle_mouse_movement(float dx, float dy) {
    m_yaw += dx * MOUSE_SENSITIVITY;
    m_pitch += dy * MOUSE_SENSITIVITY;
    m_forward.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    m_forward.y = sin(glm::radians(m_pitch));
    m_forward.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    m_forward = glm::normalize(m_forward);
    m_right = glm::normalize(glm::cross(m_forward, WORLD_UP));
}

void Camera::update(const Window &window) {
    if (glfwGetKey(*window, GLFW_KEY_W) == GLFW_PRESS) {
        m_position += m_forward * MOVEMENT_SPEED;
    }
    if (glfwGetKey(*window, GLFW_KEY_S) == GLFW_PRESS) {
        m_position -= m_forward * MOVEMENT_SPEED;
    }
    if (glfwGetKey(*window, GLFW_KEY_A) == GLFW_PRESS) {
        m_position -= m_right * MOVEMENT_SPEED;
    }
    if (glfwGetKey(*window, GLFW_KEY_D) == GLFW_PRESS) {
        m_position += m_right * MOVEMENT_SPEED;
    }
}

glm::mat4 Camera::view_matrix() const {
    return glm::lookAt(m_position, m_position + m_forward, WORLD_UP);
}
