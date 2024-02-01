#pragma once

#include <vull/maths/mat.hh>

namespace vull {

/**
 * Creates a matrix for a symmetric perspective view frustum with an infinite far plane and reverse depth.
 * @tparam T            a floating point scalar type
 * @param  aspect_ratio aspect ratio of the viewport (width over height)
 * @param  fovx         horizontal fov in radians
 * @param  near         the distance from the viewer to the near plane
 * @return              a Mat4x4 of type T
 */
template <typename T>
Mat<T, 4, 4> infinite_perspective(T aspect_ratio, T fovx, T near) {
    const T tan_half_fovy = tan(fovx / T(2)) / aspect_ratio;
    Mat<T, 4, 4> ret(T(0));
    ret[0][0] = T(1) / (aspect_ratio * tan_half_fovy);
    ret[1][1] = T(-1) / tan_half_fovy;
    ret[2][3] = T(-1);
    ret[3][2] = near;
    return ret;
}

/**
 * Creates a matrix for a symmetric perspective view frustum.
 * @tparam T            a floating point scalar type
 * @param  aspect_ratio aspect ratio of the viewport (width over height)
 * @param  fovx         horizontal fov in radians
 * @param  near         the distance from the viewer to the near plane
 * @param  far          the distance from the viewer to the far plane
 * @return              a Mat4x4 of type T
 */
template <typename T>
Mat<T, 4, 4> perspective(T aspect_ratio, T fovx, T near, T far) {
    const T tan_half_fovy = tan(fovx / T(2)) / aspect_ratio;
    Mat<T, 4, 4> ret(T(0));
    ret[0][0] = T(1) / (aspect_ratio * tan_half_fovy);
    ret[1][1] = T(-1) / tan_half_fovy;
    ret[2][2] = far / (near - far);
    ret[2][3] = T(-1);
    ret[3][2] = -(far * near) / (far - near);
    return ret;
}

/**
 * Creates a matrix for an orthographic view volume.
 * @tparam T      a floating point scalar type
 * @param  left   left plane
 * @param  right  right plane
 * @param  bottom bottom plane
 * @param  top    top plane
 * @param  near   near plane
 * @param  far    far plane
 * @return        a Mat4x4 of type T
 */
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

} // namespace vull
