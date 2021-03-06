#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

class Window;

class Camera {
    glm::vec3 m_position;
    glm::vec3 m_forward{0.0f, 0.0f, -1.0f};
    glm::vec3 m_right{0.0f, 0.0f, 0.0f};
    float m_yaw{-1.0f};
    float m_pitch{0.0f};

    void update_vectors();

public:
    explicit Camera(const glm::vec3 &position) : m_position(position) {}
    Camera(const glm::vec3 &position, float yaw, float pitch) : m_position(position), m_yaw(yaw), m_pitch(pitch) {
        update_vectors();
    }

    void handle_mouse_movement(float dx, float dy);
    void update(const Window &window, float dt);

    void set_position(const glm::vec3 &position) { m_position = position; }

    glm::mat4 view_matrix() const;
    const glm::vec3 &position() const { return m_position; }
    const glm::vec3 &forward() const { return m_forward; }
    const glm::vec3 &right() const { return m_right; }
};
