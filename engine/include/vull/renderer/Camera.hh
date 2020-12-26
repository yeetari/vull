#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

class Window;

class Camera {
    glm::vec3 m_position;
    glm::vec3 m_forward{0.0F, 0.0F, -1.0F};
    glm::vec3 m_right{0.0F, 0.0F, 0.0F};
    float m_yaw{-1.0F};
    float m_pitch{0.0F};

public:
    explicit Camera(const glm::vec3 &position) : m_position(position) {}

    void handle_mouse_movement(float dx, float dy);
    void update(const Window &window, float dt);

    glm::mat4 view_matrix() const;
    const glm::vec3 &position() const { return m_position; }
};
