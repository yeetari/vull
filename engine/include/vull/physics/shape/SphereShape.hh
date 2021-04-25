#pragma once

#include <vull/physics/shape/Shape.hh>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

class SphereShape final : public Shape {
    const float m_radius;

public:
    explicit SphereShape(float radius) : m_radius(radius) {}

    glm::mat3 inertia_tensor(float mass) const override;
    glm::vec3 support_point(const glm::vec3 &dir) const override;

    float radius() const { return m_radius; }
};
