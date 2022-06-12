#pragma once

#include <vull/maths/Vec.hh>
#include <vull/support/Array.hh>

namespace vull {

struct ContactPoint {
    Vec3f position;
    float penetration;
};

struct ContactManifold {
    Array<ContactPoint, 4> points;
    Vec3f normal;
};

struct Contact {
    Vec3f position;
    Vec3f normal;
    float penetration;
};

} // namespace vull
