#pragma once

#include <glm/mat4x4.hpp>

class Transform {
    glm::mat4 m_matrix;

public:
    explicit Transform(const glm::mat4 &matrix) : m_matrix(matrix) {}

    glm::mat4 &matrix() { return m_matrix; }
    const glm::mat4 &matrix() const { return m_matrix; }
};
