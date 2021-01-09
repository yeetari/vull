#pragma once

#include <glm/mat4x4.hpp>

class Transform {
    glm::mat4 m_matrix;

public:
    explicit Transform(const glm::mat4 &matrix) : m_matrix(matrix) {}

    glm::vec4 &position() { return m_matrix[3]; }
    const glm::vec4 &position() const { return m_matrix[3]; }

    glm::mat4 &matrix() { return m_matrix;}
    const glm::mat4 &matrix() const { return m_matrix; }
};
