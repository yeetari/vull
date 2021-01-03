#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

struct UniformBuffer {
    glm::mat4 proj;
    glm::mat4 view;
    glm::mat4 transform;
    glm::vec3 camera_position;
};
