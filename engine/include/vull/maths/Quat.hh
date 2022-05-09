#pragma once

#include <vull/maths/Mat.hh>
#include <vull/maths/Vec.hh>

namespace vull {

// TODO: Inherting from Vec whilst useful for the constructors, getters and setters is unsafe for the operation
//       functions.
template <typename T>
struct Quat : Vec<T, 4> {
    using Vec<T, 4>::Vec;
    Quat() : Vec<T, 4>(T(0), T(0), T(0), T(1)) {}
};

using Quatf = Quat<float>;

template <typename T>
Mat<T, 3, 3> to_mat3(const Quat<T> &quat) {
    Mat<T, 3, 3> ret(T(1));
    const T xx = quat.x() * quat.x();
    const T xy = quat.x() * quat.y();
    const T xz = quat.x() * quat.z();
    const T yy = quat.y() * quat.y();
    const T yz = quat.y() * quat.z();
    const T zz = quat.z() * quat.z();
    const T wx = quat.w() * quat.x();
    const T wy = quat.w() * quat.y();
    const T wz = quat.w() * quat.z();

    ret[0][0] = T(1) - T(2) * (yy + zz);
    ret[0][1] = T(2) * (xy + wz);
    ret[0][2] = T(2) * (xz - wy);

    ret[1][0] = T(2) * (xy - wz);
    ret[1][1] = T(1) - T(2) * (xx + zz);
    ret[1][2] = T(2) * (yz + wx);

    ret[2][0] = T(2) * (xz + wy);
    ret[2][1] = T(2) * (yz - wx);
    ret[2][2] = T(1) - T(2) * (xx + yy);
    return ret;
}

template <typename T>
Mat<T, 4, 4> to_mat4(const Quat<T> &quat) {
    const auto mat = to_mat3(quat);
    return Mat<T, 4, 4>(
        {Vec<T, 4>(mat[0], T(0)), Vec<T, 4>(mat[1], T(0)), Vec<T, 4>(mat[2], T(0)), Vec<T, 4>(T(0), T(0), T(0), T(1))});
}

} // namespace vull
