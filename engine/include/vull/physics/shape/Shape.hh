#pragma once

#include <glm/vec3.hpp>

class Shape {
public:
    virtual glm::vec3 support_point(const glm::vec3 &dir) const = 0;
};
