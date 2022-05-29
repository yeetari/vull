#pragma once

#include <vull/maths/Vec.hh>

#include <stdint.h>

namespace vull {

uint32_t fast_rand();
void seed_rand(uint32_t seed);

template <typename T>
struct compute_linear_rand;

template <>
struct compute_linear_rand<uint32_t> {
    uint32_t operator()(uint32_t min, uint32_t max) {
        //
        return fast_rand() % (max - min + 1) + min;
    }
};

template <>
struct compute_linear_rand<float> {
    float operator()(float min, float max) {
        //
        return float(fast_rand()) / float(UINT32_MAX) * (max - min) + min;
    }
};

template <typename T, unsigned L>
struct compute_linear_rand<Vec<T, L>> {
    Vec<T, L> operator()(const Vec<T, L> &min, const Vec<T, L> &max) {
        Vec<T, L> ret;
        for (unsigned i = 0; i < L; i++) {
            ret[i] = compute_linear_rand<T>{}(min[i], max[i]);
        }
        return ret;
    }
};

template <typename T>
T linear_rand(const T &min, const T &max) {
    return compute_linear_rand<T>{}(min, max);
}

} // namespace vull
