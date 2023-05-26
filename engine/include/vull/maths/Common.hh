#pragma once

#include <vull/support/Assert.hh>

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

template <typename T, typename U>
constexpr T lerp(T a, T b, U x) {
    return (b - a) * x + a;
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
        return T(bit_count);
    }
    if constexpr (sizeof(T) <= sizeof(unsigned)) {
        return T(__builtin_clz(value)) - (sizeof(unsigned) * 8 - bit_count);
    } else if constexpr (sizeof(T) <= sizeof(unsigned long)) {
        return T(__builtin_clzl(value)) - (sizeof(unsigned long) * 8 - bit_count);
    } else if constexpr (sizeof(T) <= sizeof(unsigned long long)) {
        return T(__builtin_clzll(value)) - (sizeof(unsigned long long) * 8 - bit_count);
    }
}

template <typename T>
constexpr T ffs(T value) {
    if (value == 0) {
        return 0;
    }
    if constexpr (sizeof(T) <= sizeof(int)) {
        return T(__builtin_ffs(static_cast<int>(value)) - 1);
    } else if constexpr (sizeof(T) <= sizeof(long)) {
        return T(__builtin_ffsl(static_cast<long>(value)) - 1);
    } else if constexpr (sizeof(T) <= sizeof(long long)) {
        return T(__builtin_ffsll(static_cast<long long>(value)) - 1);
    }
}

template <typename T>
constexpr T fls(T value) {
    if (value == 0) {
        return 0;
    }
    return T(sizeof(T) * 8) - clz(value) - T(1);
}

template <typename T>
constexpr T log2(T value) {
    return fls(value);
}

template <typename T>
constexpr T popcount(T value) {
    if constexpr (sizeof(T) <= sizeof(unsigned)) {
        return __builtin_popcount(static_cast<unsigned>(value));
    } else if constexpr (sizeof(T) <= sizeof(unsigned long)) {
        return __builtin_popcountl(static_cast<unsigned long>(value));
    } else if constexpr (sizeof(T) <= sizeof(unsigned long long)) {
        return __builtin_popcountll(static_cast<unsigned long long>(value));
    }
}

template <typename T>
constexpr T align_down(T value, T alignment) {
    VULL_ASSERT((alignment & (alignment - 1)) == 0, "Alignment not a power of two");
    return value & ~(alignment - 1);
}

template <typename T>
constexpr T align_up(T value, T alignment) {
    VULL_ASSERT((alignment & (alignment - 1)) == 0, "Alignment not a power of two");
    return (value + alignment - 1) & ~(alignment - 1);
}

inline float exp(float x) {
    return __builtin_expf(x);
}

inline float fmod(float x, float y) {
    return __builtin_fmodf(x, y);
}

inline float sin(float angle) {
    return __builtin_sinf(angle);
}

inline float cos(float angle) {
    return __builtin_cosf(angle);
}

inline float tan(float angle) {
    return __builtin_tanf(angle);
}

inline float ceil(float x) {
    return __builtin_ceilf(x);
}

inline float floor(float x) {
    return __builtin_floorf(x);
}

inline float round(float x) {
    return __builtin_roundf(x);
}

inline float sqrt(float x) {
    return __builtin_sqrtf(x);
}

inline float pow(float x, float y) {
    return __builtin_powf(x, y);
}

inline float hypot(float a, float b) {
    return sqrt(a * a + b * b);
}

} // namespace vull
