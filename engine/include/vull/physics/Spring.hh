#pragma once

#include <glm/vec3.hpp>

class PhysicsSystem;

class Spring {
    friend PhysicsSystem;

private:
    const glm::vec3 m_point;
    const float m_rest_length;
    const float m_spring_constant;

public:
    Spring(const glm::vec3 &point, float rest_length, float spring_constant)
        : m_point(point), m_rest_length(rest_length), m_spring_constant(spring_constant) {}
};
