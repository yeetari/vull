#pragma once

#include <vull/physics/shape/Shape.hh>

class SphereShape final : public Shape {
    const float m_radius;

public:
    explicit SphereShape(float radius) : m_radius(radius) {}

    glm::vec3 support_point(const glm::vec3 &dir) const override { return glm::normalize(dir) * m_radius; }

    float radius() const { return m_radius; }
};
