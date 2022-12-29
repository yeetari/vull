#pragma once

#include <vull/maths/Mat.hh>
#include <vull/maths/Vec.hh>

namespace vull {

template <typename T>
class Quat {
    T m_x{T(0)};
    T m_y{T(0)};
    T m_z{T(0)};
    T m_w{T(1)};

public:
    static constexpr unsigned length = 4;

    constexpr Quat() = default;
    template <typename U>
    constexpr Quat(U x, U y, U z, U w) : m_x(T(x)), m_y(T(y)), m_z(T(z)), m_w(T(w)) {}
    constexpr Quat(const Vec<T, 3> &xyz, T w) : m_x(xyz.x()), m_y(xyz.y()), m_z(xyz.z()), m_w(w) {}
    constexpr Quat(const Vec<T, 4> &xyzw) : m_x(xyzw.x()), m_y(xyzw.y()), m_z(xyzw.z()), m_w(xyzw.w()) {}

    void set_x(T x) { m_x = x; }
    void set_y(T y) { m_y = y; }
    void set_z(T z) { m_z = z; }
    void set_w(T w) { m_w = w; }

    constexpr T &operator[](unsigned index);
    constexpr T x() const { return m_x; }
    constexpr T y() const { return m_y; }
    constexpr T z() const { return m_z; }
    constexpr T w() const { return m_w; }
};

using Quatf = Quat<float>;

template <typename T>
constexpr T &Quat<T>::operator[](unsigned index) {
    switch (index) {
    case 0:
        return m_x;
    case 1:
        return m_y;
    case 2:
        return m_z;
    case 3:
        return m_w;
    default:
        vull::unreachable();
    }
}

template <typename T>
constexpr Quat<T> operator+(const Quat<T> &lhs, const Quat<T> &rhs) {
    return Quat<T>(lhs.x() + rhs.x(), lhs.y() + rhs.y(), lhs.z() + rhs.z(), lhs.w() + rhs.w());
}

template <typename T>
constexpr Quat<T> operator*(const Quat<T> &lhs, T rhs) {
    return Quat<T>(lhs.x() * rhs, lhs.y() * rhs, lhs.z() * rhs, lhs.w() * rhs);
}

template <typename T>
constexpr Quat<T> operator/(const Quat<T> &lhs, T rhs) {
    return Quat<T>(lhs.x() / rhs, lhs.y() / rhs, lhs.z() / rhs, lhs.w() / rhs);
}

template <typename T>
constexpr Quat<T> operator*(const Quat<T> &lhs, const Quat<T> &rhs) {
    return {
        lhs.w() * rhs.x() + lhs.x() * rhs.w() + lhs.y() * rhs.z() - lhs.z() * rhs.y(),
        lhs.w() * rhs.y() + lhs.y() * rhs.w() + lhs.z() * rhs.x() - lhs.x() * rhs.z(),
        lhs.w() * rhs.z() + lhs.z() * rhs.w() + lhs.x() * rhs.y() - lhs.y() * rhs.x(),
        lhs.w() * rhs.w() - lhs.x() * rhs.x() - lhs.y() * rhs.y() - lhs.z() * rhs.z(),
    };
}

template <typename T>
constexpr Quat<T> angle_axis(T angle, const Vec<T, 3> &axis) {
    T half_angle = angle * T(0.5);
    return Quat<T>(axis * sin(half_angle), cos(half_angle));
}

template <typename T>
constexpr Quat<T> conjugate(const Quat<T> &quat) {
    return Quat<T>(-quat.x(), -quat.y(), -quat.z(), quat.w());
}

template <typename T>
constexpr T dot(const Quat<T> &lhs, const Quat<T> &rhs) {
    return lhs.x() * rhs.x() + lhs.y() * rhs.y() + lhs.z() * rhs.z() + lhs.w() * rhs.w();
}

template <typename T>
constexpr Quat<T> inverse(const Quat<T> &quat) {
    return conjugate(quat) / square_magnitude(quat);
}

template <typename T>
constexpr T magnitude(const Quat<T> &quat) {
    return sqrt(square_magnitude(quat));
}

template <typename T>
constexpr Quat<T> normalise(const Quat<T> &quat) {
    T mag = magnitude(quat);
    if (mag <= T(0)) {
        return {};
    }
    return quat / mag;
}

// Faster quaternion-vector rotation from
// https://blog.molecular-matters.com/2013/05/24/a-faster-quaternion-vector-multiplication/
template <typename T>
constexpr Vec<T, 3> rotate(const Quat<T> &quat, const Vec<T, 3> &vec) {
    Vec<T, 3> quat_vec(quat.x(), quat.y(), quat.z());
    auto t = cross(quat_vec, vec) * T(2);
    return vec + t * quat.w() + cross(quat_vec, t);
}

template <typename T>
constexpr T square_magnitude(const Quat<T> &quat) {
    return dot(quat, quat);
}

template <typename T>
constexpr Mat<T, 3, 3> to_mat3(const Quat<T> &quat) {
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
constexpr Mat<T, 4, 4> to_mat4(const Quat<T> &quat) {
    const auto mat = to_mat3(quat);
    return Mat<T, 4, 4>(
        {Vec<T, 4>(mat[0], T(0)), Vec<T, 4>(mat[1], T(0)), Vec<T, 4>(mat[2], T(0)), Vec<T, 4>(T(0), T(0), T(0), T(1))});
}

} // namespace vull
