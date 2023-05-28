#pragma once

#include <vull/maths/Relational.hh>
#include <vull/maths/Vec.hh>

namespace vull {

template <typename T>
inline constexpr T k_fixed_epsilon;
template <>
inline constexpr float k_fixed_epsilon<float> = 1e-5f;
template <>
inline constexpr double k_fixed_epsilon<double> = 1e-10;

template <typename T, unsigned L>
constexpr Vec<bool, L> epsilon_equal(const Vec<T, L> &lhs, const Vec<T, L> &rhs, const Vec<T, L> &epsilon) {
    return less_than_equal(abs(lhs - rhs), epsilon);
}

template <typename T, unsigned L>
constexpr bool fuzzy_equal(const Vec<T, L> &lhs, const Vec<T, L> &rhs) {
    const auto near_zero = less_than_equal(abs(lhs - rhs), Vec<T, L>(k_fixed_epsilon<T>));
    const auto epsilon_factor = select(max(abs(lhs), abs(rhs)), Vec<T, L>(T(1)), near_zero);
    return all(epsilon_equal(lhs, rhs, epsilon_factor * k_fixed_epsilon<T>));
}

template <typename T, unsigned L>
constexpr bool fuzzy_zero(const Vec<T, L> &vec) {
    return all(epsilon_equal(vec, Vec<T, L>(0), Vec<T, L>(k_fixed_epsilon<T>)));
}

template <typename T>
constexpr bool epsilon_equal(T lhs, T rhs, T epsilon) {
    return epsilon_equal(Vec<T, 1>(lhs), Vec<T, 1>(rhs), Vec<T, 1>(epsilon)).x();
}

template <typename T>
constexpr bool fuzzy_equal(T lhs, T rhs) {
    return fuzzy_equal(Vec<T, 1>(lhs), Vec<T, 1>(rhs));
}

template <typename T>
constexpr bool fuzzy_zero(T value) {
    return fuzzy_zero(Vec<T, 1>(value));
}

} // namespace vull
