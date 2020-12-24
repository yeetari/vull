#include <renderer/Camera.hh>

#include <Window.hh>

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

namespace {

constexpr float MOUSE_SENSITIVITY = 0.002F;
constexpr float MOVEMENT_SPEED = 10.0F;
constexpr float MIN_PITCH = -glm::half_pi<float>() + glm::radians(1.0F);
constexpr float MAX_PITCH = glm::half_pi<float>() - glm::radians(1.0F);
constexpr glm::vec3 WORLD_UP(0.0F, 1.0F, 0.0F);

} // namespace

void Camera::handle_mouse_movement(float dx, float dy) {
    m_yaw += dx * MOUSE_SENSITIVITY;
    m_pitch += dy * MOUSE_SENSITIVITY;
    m_pitch = glm::clamp(m_pitch, MIN_PITCH, MAX_PITCH);
    m_forward.x = glm::cos(m_yaw) * glm::cos(m_pitch);
    m_forward.y = glm::sin(m_pitch);
    m_forward.z = glm::sin(m_yaw) * glm::cos(m_pitch);
    m_forward = glm::normalize(m_forward);
    m_right = glm::normalize(glm::cross(m_forward, WORLD_UP));
}

void Camera::update(const Window &window, float dt) {
    float speed = dt * MOVEMENT_SPEED;
    if (glfwGetKey(*window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
        speed *= 4;
    }
    if (glfwGetKey(*window, GLFW_KEY_W) == GLFW_PRESS) {
        m_position += m_forward * speed;
    }
    if (glfwGetKey(*window, GLFW_KEY_S) == GLFW_PRESS) {
        m_position -= m_forward * speed;
    }
    if (glfwGetKey(*window, GLFW_KEY_A) == GLFW_PRESS) {
        m_position -= m_right * speed;
    }
    if (glfwGetKey(*window, GLFW_KEY_D) == GLFW_PRESS) {
        m_position += m_right * speed;
    }
}

glm::mat4 Camera::view_matrix() const {
    return glm::lookAt(m_position, m_position + m_forward, WORLD_UP);
}
