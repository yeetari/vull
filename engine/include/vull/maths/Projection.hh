#pragma once

#include <vull/maths/Mat.hh>

namespace vull {

// Infinite, reversed depth perspective projection.
template <typename T>
Mat<T, 4, 4> infinite_perspective(T aspect_ratio, T fov, T near) {
    const T tan_half_fov = tan(fov / 2.0f) / aspect_ratio;
    Mat<T, 4, 4> ret(T(0));
    ret[0][0] = T(1) / (aspect_ratio * tan_half_fov);
    ret[1][1] = T(-1) / tan_half_fov;
    ret[2][3] = T(-1);
    ret[3][2] = near;
    return ret;
}

template <typename T>
Mat<T, 4, 4> ortho(T left, T right, T bottom, T top, T near, T far) {
    Mat<T, 4, 4> ret(T(1));
    ret[0][0] = T(2) / (right - left);
    ret[1][1] = T(-2) / (top - bottom);
    ret[2][2] = T(-1) / (far - near);
    ret[3][0] = -(right + left) / (right - left);
    ret[3][1] = -(top + bottom) / (top - bottom);
    ret[3][2] = -near / (far - near);
    return ret;
}

template <typename T>
Mat<T, 4, 4> perspective(T aspect_ratio, T fov, T near, T far) {
    const T tan_half_fov = tan(fov / 2.0f) / aspect_ratio;
    Mat<T, 4, 4> ret(T(0));
    ret[0][0] = T(1) / (aspect_ratio * tan_half_fov);
    ret[1][1] = T(-1) / tan_half_fov;
    ret[2][2] = far / (near - far);
    ret[2][3] = T(-1);
    ret[3][2] = -(far * near) / (far - near);
    return ret;
}

} // namespace vull
