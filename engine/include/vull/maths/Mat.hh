#pragma once

#include <vull/container/Array.hh>
#include <vull/maths/Common.hh>
#include <vull/maths/Vec.hh>

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

template <typename T, unsigned R>
Vec<T, R> operator*(const Vec<T, R> &lhs, const Mat<T, R, R> &rhs) {
    Vec<T, R> ret;
    for (unsigned i = 0; i < R; i++) {
        for (unsigned j = 0; j < R; j++) {
            ret[i] += lhs[j] * rhs[i][j];
        }
    }
    return ret;
}

template <typename T>
Mat<T, 3, 3> inverse(const Mat<T, 3, 3> &mat) {
    T one_over_det = T(1) / (mat[0][0] * (mat[1][1] * mat[2][2] - mat[2][1] * mat[1][2]) -
                             mat[1][0] * (mat[0][1] * mat[2][2] - mat[2][1] * mat[0][2]) +
                             mat[2][0] * (mat[0][1] * mat[1][2] - mat[1][1] * mat[0][2]));

    Mat<T, 3, 3> ret;
    ret[0][0] = +(mat[1][1] * mat[2][2] - mat[2][1] * mat[1][2]) * one_over_det;
    ret[1][0] = -(mat[1][0] * mat[2][2] - mat[2][0] * mat[1][2]) * one_over_det;
    ret[2][0] = +(mat[1][0] * mat[2][1] - mat[2][0] * mat[1][1]) * one_over_det;
    ret[0][1] = -(mat[0][1] * mat[2][2] - mat[2][1] * mat[0][2]) * one_over_det;
    ret[1][1] = +(mat[0][0] * mat[2][2] - mat[2][0] * mat[0][2]) * one_over_det;
    ret[2][1] = -(mat[0][0] * mat[2][1] - mat[2][0] * mat[0][1]) * one_over_det;
    ret[0][2] = +(mat[0][1] * mat[1][2] - mat[1][1] * mat[0][2]) * one_over_det;
    ret[1][2] = -(mat[0][0] * mat[1][2] - mat[1][0] * mat[0][2]) * one_over_det;
    ret[2][2] = +(mat[0][0] * mat[1][1] - mat[1][0] * mat[0][1]) * one_over_det;
    return ret;
}

template <typename T>
Mat<T, 4, 4> inverse(const Mat<T, 4, 4> &mat) {
    T coef00 = mat[2][2] * mat[3][3] - mat[3][2] * mat[2][3];
    T coef02 = mat[1][2] * mat[3][3] - mat[3][2] * mat[1][3];
    T coef03 = mat[1][2] * mat[2][3] - mat[2][2] * mat[1][3];

    T coef04 = mat[2][1] * mat[3][3] - mat[3][1] * mat[2][3];
    T coef06 = mat[1][1] * mat[3][3] - mat[3][1] * mat[1][3];
    T coef07 = mat[1][1] * mat[2][3] - mat[2][1] * mat[1][3];

    T coef08 = mat[2][1] * mat[3][2] - mat[3][1] * mat[2][2];
    T coef10 = mat[1][1] * mat[3][2] - mat[3][1] * mat[1][2];
    T coef11 = mat[1][1] * mat[2][2] - mat[2][1] * mat[1][2];

    T coef12 = mat[2][0] * mat[3][3] - mat[3][0] * mat[2][3];
    T coef14 = mat[1][0] * mat[3][3] - mat[3][0] * mat[1][3];
    T coef15 = mat[1][0] * mat[2][3] - mat[2][0] * mat[1][3];

    T coef16 = mat[2][0] * mat[3][2] - mat[3][0] * mat[2][2];
    T coef18 = mat[1][0] * mat[3][2] - mat[3][0] * mat[1][2];
    T coef19 = mat[1][0] * mat[2][2] - mat[2][0] * mat[1][2];

    T coef20 = mat[2][0] * mat[3][1] - mat[3][0] * mat[2][1];
    T coef22 = mat[1][0] * mat[3][1] - mat[3][0] * mat[1][1];
    T coef23 = mat[1][0] * mat[2][1] - mat[2][0] * mat[1][1];

    Vec<T, 4> fac0(coef00, coef00, coef02, coef03);
    Vec<T, 4> fac1(coef04, coef04, coef06, coef07);
    Vec<T, 4> fac2(coef08, coef08, coef10, coef11);
    Vec<T, 4> fac3(coef12, coef12, coef14, coef15);
    Vec<T, 4> fac4(coef16, coef16, coef18, coef19);
    Vec<T, 4> fac5(coef20, coef20, coef22, coef23);

    Vec<T, 4> vec0(mat[1][0], mat[0][0], mat[0][0], mat[0][0]);
    Vec<T, 4> vec1(mat[1][1], mat[0][1], mat[0][1], mat[0][1]);
    Vec<T, 4> vec2(mat[1][2], mat[0][2], mat[0][2], mat[0][2]);
    Vec<T, 4> vec3(mat[1][3], mat[0][3], mat[0][3], mat[0][3]);

    Vec<T, 4> inv0(vec1 * fac0 - vec2 * fac1 + vec3 * fac2);
    Vec<T, 4> inv1(vec0 * fac0 - vec2 * fac3 + vec3 * fac4);
    Vec<T, 4> inv2(vec0 * fac1 - vec1 * fac3 + vec3 * fac5);
    Vec<T, 4> inv3(vec0 * fac2 - vec1 * fac4 + vec2 * fac5);

    Vec<T, 4> signA(+1, -1, +1, -1);
    Vec<T, 4> signB(-1, +1, -1, +1);
    Mat<T, 4, 4> inverse({inv0 * signA, inv1 * signB, inv2 * signA, inv3 * signB});

    Vec<T, 4> row0(inverse[0][0], inverse[1][0], inverse[2][0], inverse[3][0]);
    Vec<T, 4> dot0(mat[0] * row0);
    T dot1 = (dot0.x() + dot0.y()) + (dot0.z() + dot0.w());
    return inverse * (T(1) / dot1);
}

template <typename T, unsigned C, unsigned R>
Mat<T, R, C> transpose(const Mat<T, C, R> &lhs) {
    Mat<T, R, C> ret;
    for (unsigned i = 0; i < C; i++) {
        for (unsigned j = 0; j < R; j++) {
            ret[i][j] = lhs[j][i];
        }
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

template <typename T>
Mat<T, 4, 4> rotation_x(T angle) {
    Mat<T, 4, 4> ret(T(1));
    ret[1][1] = ret[2][2] = cos(angle);
    ret[2][1] = -(ret[1][2] = sin(angle));
    return ret;
}

template <typename T>
Mat<T, 4, 4> rotation_y(T angle) {
    Mat<T, 4, 4> ret(T(1));
    ret[0][0] = ret[2][2] = cos(angle);
    ret[0][2] = -(ret[2][0] = sin(angle));
    return ret;
}

template <typename T>
Mat<T, 4, 4> rotation_z(T angle) {
    Mat<T, 4, 4> ret(T(1));
    ret[0][0] = ret[1][1] = cos(angle);
    ret[1][0] = -(ret[0][1] = sin(angle));
    return ret;
}

template <typename T>
Mat<T, 4, 4> rotation(T angle, const Vec<T, 3> &axis) {
    T cos_angle = cos(angle);
    T sin_angle = sin(angle);

    Vec<T, 3> axis_cos = axis * (T(1) - cos_angle);
    Vec<T, 3> axis_sin = axis * sin_angle;
    Mat<T, 4, 4> ret(T(1));
    ret[0][0] = cos_angle + axis.x() * axis_cos.x();
    ret[0][1] = axis.y() * axis_cos.x() + axis_sin.z();
    ret[0][2] = axis.z() * axis_cos.x() - axis_sin.y();
    ret[1][0] = axis.x() * axis_cos.y() - axis_sin.z();
    ret[1][1] = cos_angle + axis.y() * axis_cos.y();
    ret[1][2] = axis.z() * axis_cos.y() + axis_sin.x();
    ret[2][0] = axis.x() * axis_cos.z() + axis_sin.y();
    ret[2][1] = axis.y() * axis_cos.z() - axis_sin.x();
    ret[2][2] = cos_angle + axis.z() * axis_cos.z();
    return ret;
}

} // namespace vull
