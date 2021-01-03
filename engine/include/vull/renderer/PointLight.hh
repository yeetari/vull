#pragma once

#include <glm/vec3.hpp>

struct PointLight {
    glm::vec3 position;
    float radius;
    glm::vec3 colour;
    float padding;
};
