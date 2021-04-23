#pragma once

#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

class Transform {
    glm::vec3 m_position;
    glm::quat m_orientation{};
    glm::vec3 m_scale{1.0f};

public:
    explicit Transform(const glm::vec3 &position) : m_position(position) {}
    Transform(const glm::vec3 &position, const glm::vec3 &scale) : m_position(position), m_scale(scale) {}

    glm::mat4 matrix() const;
    glm::mat4 scaled_matrix() const;

    glm::vec3 &position() { return m_position; }
    const glm::vec3 &position() const { return m_position; }

    glm::quat &orientation() { return m_orientation; }
    const glm::quat &orientation() const { return m_orientation; }

    glm::vec3 &scale() { return m_scale; }
    const glm::vec3 &scale() const { return m_scale; }
};
