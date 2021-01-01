#include <vull/renderer/Camera.hh>

#include <vull/io/Window.hh>

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

namespace {

constexpr float k_mouse_sensitivity = 0.002F;
constexpr float k_movement_speed = 10.0F;
constexpr float k_min_pitch = -glm::half_pi<float>() + glm::radians(1.0F);
constexpr float k_max_pitch = glm::half_pi<float>() - glm::radians(1.0F);
constexpr glm::vec3 k_world_up(0.0F, 1.0F, 0.0F);

} // namespace

void Camera::update_vectors() {
    m_forward.x = glm::cos(m_yaw) * glm::cos(m_pitch);
    m_forward.y = glm::sin(m_pitch);
    m_forward.z = glm::sin(m_yaw) * glm::cos(m_pitch);
    m_forward = glm::normalize(m_forward);
    m_right = glm::normalize(glm::cross(m_forward, k_world_up));
}

void Camera::handle_mouse_movement(float dx, float dy) {
    m_yaw += dx * k_mouse_sensitivity;
    m_pitch += dy * k_mouse_sensitivity;
    m_pitch = glm::clamp(m_pitch, k_min_pitch, k_max_pitch);
    update_vectors();
}

void Camera::update(const Window &window, float dt) {
    float speed = dt * k_movement_speed;
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
    return glm::lookAt(m_position, m_position + m_forward, k_world_up);
}
