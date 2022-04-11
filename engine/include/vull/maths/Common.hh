#pragma once

#include <math.h>

namespace vull {

template <typename T>
constexpr T half_pi = T(1.57079632679489661923132169163975144);

template <typename T>
constexpr T pi = T(3.14159265358979323846264338327950288);

template <typename T>
constexpr T abs(T x) {
    return x >= T(0) ? x : -x;
}

template <typename T>
constexpr T lerp(T a, T b, T x) {
    return x * (a - b) + b;
}

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

constexpr float sin(float angle) {
    return ::sinf(angle);
}

constexpr float cos(float angle) {
    return ::cosf(angle);
}

constexpr float tan(float angle) {
    return ::tanf(angle);
}

constexpr float sqrt(float x) {
    return ::sqrtf(x);
}

constexpr float pow(float x, float y) {
    return ::powf(x, y);
}

constexpr float hypot(float a, float b) {
    return sqrt(a * a + b * b);
}

} // namespace vull
