#pragma once

#include <vull/support/Vector.hh>

#include <glm/vec3.hpp>

struct BezierCurve {
    static Vector<glm::vec3> construct(const Vector<glm::vec3> &control_points, float resolution);
};
