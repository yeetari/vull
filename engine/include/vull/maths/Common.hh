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
constexpr T ceil_div(T x, T y) {
    return x / y + T(x % y != 0);
}

template <typename T>
constexpr T lerp(T a, T b, T x) {
    return x * (a - b) + b;
}

template <typename T>
constexpr T sign(T x) {
    return T(T(0) < x) - T(x < T(0));
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

template <typename T>
constexpr T clz(T value) {
    constexpr int bit_count = sizeof(T) * 8;
    if (value == 0) {
        return bit_count;
    }
    if constexpr (sizeof(T) <= sizeof(unsigned)) {
        return T(__builtin_clz(value)) - (sizeof(unsigned) * 8 - bit_count);
    } else if constexpr (sizeof(T) <= sizeof(unsigned long)) {
        return T(__builtin_clzl(value)) - (sizeof(unsigned long) * 8 - bit_count);
    } else if constexpr (sizeof(T) <= sizeof(unsigned long long)) {
        return T(__builtin_clzll(value)) - (sizeof(unsigned long long) * 8 - bit_count);
    }
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

constexpr float ceil(float x) {
    return ::ceilf(x);
}

constexpr float floor(float x) {
    return ::floorf(x);
}

constexpr float round(float x) {
    return ::roundf(x);
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
