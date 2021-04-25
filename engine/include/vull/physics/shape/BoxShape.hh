#pragma once

#include <vull/physics/shape/Shape.hh>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

class BoxShape final : public Shape {
    const glm::vec3 m_half_size;

public:
    explicit BoxShape(const glm::vec3 &m_half_size) : m_half_size(m_half_size) {}

    glm::mat3 inertia_tensor(float mass) const override;
    glm::vec3 support_point(const glm::vec3 &dir) const override;

    const glm::vec3 &half_size() const { return m_half_size; }
};
