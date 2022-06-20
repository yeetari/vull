#include <vull/core/Transform.hh>

#include <vull/maths/Common.hh>
#include <vull/maths/Mat.hh>
#include <vull/maths/Quat.hh>
#include <vull/maths/Vec.hh>

namespace vull {

Vec3f Transform::forward() const {
    return vull::rotate(m_rotation, Vec3f(0.0f, 0.0f, 1.0f));
}

Vec3f Transform::right() const {
    return vull::rotate(m_rotation, Vec3f(1.0f, 0.0f, 0.0f));
}

Vec3f Transform::up() const {
    return vull::rotate(m_rotation, Vec3f(0.0f, 1.0f, 0.0f));
}

Mat4f Transform::matrix() const {
    Mat4f ret(1.0f);
    ret[3] = Vec4f(m_position, 1.0f);
    ret = ret * vull::to_mat4(m_rotation);
    ret[0] *= m_scale.x();
    ret[1] *= m_scale.y();
    ret[2] *= m_scale.z();
    return ret;
}

} // namespace vull
