#include <vull/physics/shape/BoxShape.hh>

#include <glm/common.hpp>

glm::mat3 BoxShape::inertia_tensor(const float mass) const {
    const float x2 = m_half_size.x * m_half_size.x;
    const float y2 = m_half_size.y * m_half_size.y;
    const float z2 = m_half_size.z * m_half_size.z;
    return glm::mat3{
        (y2 + z2) / 3.0f * mass, 0.0f, 0.0f, 0.0f, (x2 + z2) / 3.0f * mass, 0.0f, 0.0f, 0.0f, (x2 + y2) / 3.0f * mass,
    };
}

glm::vec3 BoxShape::support_point(const glm::vec3 &dir) const {
    glm::vec3 ret;
    ret.x = glm::sign(dir.x) * m_half_size.x;
    ret.y = glm::sign(dir.y) * m_half_size.y;
    ret.z = glm::sign(dir.z) * m_half_size.z;
    return ret;
}
