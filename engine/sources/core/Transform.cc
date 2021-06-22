#include <vull/core/Transform.hh>

#include <glm/gtc/matrix_transform.hpp>

glm::mat4 Transform::matrix() const {
    return glm::translate(glm::mat4(1.0f), m_position) * glm::mat4_cast(m_orientation);
}

glm::mat4 Transform::scaled_matrix() const {
    return glm::scale(matrix(), m_scale);
}

glm::vec3 Transform::forward() const {
    return m_orientation * glm::vec3(0.0f, 0.0f, 1.0f);
}

glm::vec3 Transform::right() const {
    return m_orientation * glm::vec3(1.0f, 0.0f, 0.0f);
}

glm::vec3 Transform::up() const {
    return m_orientation * glm::vec3(0.0f, 1.0f, 0.0f);
}

glm::vec3 Transform::local_to_world(const glm::vec3 &point) const {
    return m_position + (m_orientation * point);
}

Transform Transform::translated(const glm::vec3 &translation) const {
    return {m_position + m_orientation * translation, m_orientation, m_scale};
}
