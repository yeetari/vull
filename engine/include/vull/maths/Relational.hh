#pragma once

#include <vull/maths/Vec.hh>

namespace vull {

template <unsigned L>
constexpr bool all(const Vec<bool, L> &vec) {
    bool ret = true;
    for (unsigned i = 0; i < L; i++) {
        ret &= vec[i];
    }
    return ret;
}

template <unsigned L>
constexpr bool any(const Vec<bool, L> &vec) {
    bool ret = false;
    for (unsigned i = 0; i < L; i++) {
        ret |= vec[i];
    }
    return ret;
}

#define DEFINE_CMP(name, op)                                                                                           \
    template <typename T, unsigned L>                                                                                  \
    constexpr Vec<bool, L> name(const Vec<T, L> &lhs, const Vec<T, L> &rhs) {                                          \
        Vec<bool, L> ret;                                                                                              \
        for (unsigned i = 0; i < L; i++) {                                                                             \
            ret[i] = lhs[i] op rhs[i];                                                                                 \
        }                                                                                                              \
        return ret;                                                                                                    \
    }

DEFINE_CMP(equal, ==)
DEFINE_CMP(not_equal, !=)
DEFINE_CMP(less_than, <)
DEFINE_CMP(greater_than, >)
DEFINE_CMP(less_than_equal, <=)
DEFINE_CMP(greater_than_equal, >=)
#undef DEFINE_CMP

} // namespace vull
