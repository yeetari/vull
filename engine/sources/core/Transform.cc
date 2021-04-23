#include <vull/core/Transform.hh>

#include <glm/gtc/matrix_transform.hpp>

glm::mat4 Transform::matrix() const {
    return glm::translate(glm::mat4(1.0f), m_position) * glm::mat4_cast(m_orientation);
}

glm::mat4 Transform::scaled_matrix() const {
    return glm::scale(matrix(), m_scale);
}
