#include <vull/physics/shape.hh>

#include <vull/maths/mat.hh>
#include <vull/maths/vec.hh>

namespace vull {

Vec3f BoxShape::furthest_point(const Vec3f &direction) const {
    return m_half_extents * vull::sign(direction);
}

Mat3f BoxShape::inertia_tensor(float mass) const {
    Vec3f e2 = m_half_extents * m_half_extents;
    return Mat3f{{
        Vec3f{(e2.y() + e2.z()) / 3.0f * mass, 0.0f, 0.0f},
        Vec3f{0.0f, (e2.x() + e2.z()) / 3.0f * mass, 0.0f},
        Vec3f{0.0f, 0.0f, (e2.x() + e2.y()) / 3.0f * mass},
    }};
}

} // namespace vull
