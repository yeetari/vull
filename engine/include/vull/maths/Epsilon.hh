#pragma once

#include <vull/maths/Relational.hh>
#include <vull/maths/Vec.hh>

namespace vull {

template <typename T>
inline constexpr T k_fixed_epsilon;
template <>
inline constexpr float k_fixed_epsilon<float> = 1e-5f;

template <typename T, unsigned L>
Vec<bool, L> epsilon_equal(const Vec<T, L> &lhs, const Vec<T, L> &rhs, T epsilon) {
    return epsilon_equal(lhs, rhs, Vec<T, L>(epsilon));
}

template <typename T, unsigned L>
Vec<bool, L> epsilon_equal(const Vec<T, L> &lhs, const Vec<T, L> &rhs, const Vec<T, L> &epsilon) {
    return less_than_equal(abs(lhs - rhs), epsilon);
}

template <typename T, unsigned L>
bool fuzzy_equal(const Vec<T, L> &lhs, const Vec<T, L> &rhs) {
    auto epsilon_factor = min(max(abs(lhs), abs(rhs)), Vec<T, L>(T(1)));
    return all(epsilon_equal(lhs, rhs, epsilon_factor * k_fixed_epsilon<T>));
}

template <typename T, unsigned L>
bool fuzzy_zero(const Vec<T, L> &vec) {
    return all(epsilon_equal(vec, Vec<T, L>(T(0)), k_fixed_epsilon<T>));
}

} // namespace vull
