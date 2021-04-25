#pragma once

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

class Shape {
public:
    virtual glm::mat3 inertia_tensor(float mass) const = 0;
    virtual glm::vec3 support_point(const glm::vec3 &dir) const = 0;
};
