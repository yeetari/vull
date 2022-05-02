#pragma once

#include <vull/maths/Common.hh>
#include <vull/maths/Vec.hh>
#include <vull/support/Array.hh>

namespace vull {

template <typename T, unsigned C, unsigned R>
class Mat {
    Array<Vec<T, R>, C> m_cols;

public:
    constexpr Mat() : Mat(T(0)) {}
    constexpr Mat(T t);
    constexpr Mat(const Array<Vec<T, R>, C> &cols) : m_cols(cols) {}

    Vec<T, R> &operator[](unsigned col) { return m_cols[col]; }
    const Vec<T, R> &operator[](unsigned col) const { return m_cols[col]; }
};

using Mat3x3f = Mat<float, 3, 3>;
using Mat4x4f = Mat<float, 4, 4>;
using Mat3f = Mat3x3f;
using Mat4f = Mat4x4f;

template <typename T, unsigned C, unsigned R>
constexpr Mat<T, C, R>::Mat(T t) : m_cols{} {
    for (unsigned i = 0; i < C; i++) {
        m_cols[i][i] = t;
    }
}

template <typename T, unsigned C, unsigned R>
Mat<T, C, R> operator*(const Mat<T, C, R> &lhs, T rhs) {
    Mat<T, C, R> ret;
    for (unsigned i = 0; i < C; i++) {
        ret[i] = lhs[i] * rhs;
    }
    return ret;
}

template <typename T, unsigned C, unsigned R, unsigned RhsC>
Mat<T, RhsC, R> operator*(const Mat<T, C, R> &lhs, const Mat<T, RhsC, C> &rhs) {
    Mat<T, RhsC, R> ret;
    for (unsigned col = 0; col < RhsC; col++) {
        for (unsigned i = 0; i < C; i++) {
            ret[col] += lhs[i] * rhs[col][i];
        }
    }
    return ret;
}

template <typename T, unsigned C>
Vec<T, C> operator*(const Mat<T, C, C> &lhs, const Vec<T, C> &rhs) {
    Vec<T, C> ret;
    for (unsigned i = 0; i < C; i++) {
        ret += lhs[i] * rhs[i];
    }
    return ret;
}

template <typename T>
Mat<T, 4, 4> look_at(const Vec<T, 3> &camera, const Vec<T, 3> &center, const Vec<T, 3> &up) {
    const auto f = normalise(center - camera);
    const auto s = normalise(cross(f, up));
    const auto u = cross(s, f);
    Mat<T, 4, 4> ret(T(1));
    ret[0][0] = s.x();
    ret[1][0] = s.y();
    ret[2][0] = s.z();
    ret[0][1] = u.x();
    ret[1][1] = u.y();
    ret[2][1] = u.z();
    ret[0][2] = -f.x();
    ret[1][2] = -f.y();
    ret[2][2] = -f.z();
    ret[3][0] = -dot(s, camera);
    ret[3][1] = -dot(u, camera);
    ret[3][2] = dot(f, camera);
    return ret;
}

// Infinite, reversed depth perspective projection.
// TODO: Take in horizontal_fov?
template <typename T>
Mat<T, 4, 4> infinite_perspective(T aspect_ratio, T near_plane, T vertical_fov) {
    const T tan_half_fov = vull::tan(vertical_fov / T(2));
    Mat<T, 4, 4> ret(T(0));
    ret[0][0] = T(1) / (aspect_ratio * tan_half_fov);
    ret[1][1] = T(-1) / tan_half_fov;
    ret[2][3] = T(-1);
    ret[3][2] = near_plane;
    return ret;
}

template <typename T>
Mat<T, 4, 4> perspective(T aspect_ratio, T near_plane, T far_plane, T vertical_fov) {
    const T tan_half_fov = vull::tan(vertical_fov / T(2));
    Mat<T, 4, 4> ret(T(0));
    ret[0][0] = T(1) / (aspect_ratio * tan_half_fov);
    ret[1][1] = T(-1) / tan_half_fov;
    ret[2][2] = far_plane / (near_plane - far_plane);
    ret[2][3] = T(-1);
    ret[3][2] = -(far_plane * near_plane) / (far_plane - near_plane);
    return ret;
}

} // namespace vull
