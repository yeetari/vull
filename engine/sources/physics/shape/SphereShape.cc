#include <vull/physics/shape/SphereShape.hh>

#include <glm/geometric.hpp>

glm::mat3 SphereShape::inertia_tensor(const float mass) const {
    const float mr_sqrd = 0.4f * mass * m_radius * m_radius;
    return glm::mat3{
        mr_sqrd, 0.0f, 0.0f, 0.0f, mr_sqrd, 0.0f, 0.0f, 0.0f, mr_sqrd,
    };
}

glm::vec3 SphereShape::support_point(const glm::vec3 &dir) const {
    return glm::normalize(dir) * m_radius;
}
