#pragma once

namespace vull {

template <typename T>
constexpr T min(T a, T b) {
    return b < a ? b : a;
}

template <typename T>
constexpr T max(T a, T b) {
    return a < b ? b : a;
}

template <typename T>
constexpr T clamp(T val, T min_val, T max_val) {
    return min(max(val, min_val), max_val);
}

} // namespace vull
