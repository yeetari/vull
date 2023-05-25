#pragma once

#include <vull/container/Array.hh>
#include <vull/maths/Common.hh>

namespace vull {

#define ENUMERATE_OPS(O)                                                                                               \
    O(+, +=)                                                                                                           \
    O(-, -=)                                                                                                           \
    O(*, *=)                                                                                                           \
    O(/, /=)                                                                                                           \
    O(%, %=)                                                                                                           \
    O(&, &=)                                                                                                           \
    O(|, |=)                                                                                                           \
    O(^, ^=)                                                                                                           \
    O(<<, <<=)                                                                                                         \
    O(>>, >>=)

template <typename Derived>
struct VecBase {
#define ENUMERATE_OP(op, ope)                                                                                          \
    constexpr Derived operator op(const Derived &rhs) const {                                                          \
        return Derived(static_cast<const Derived &>(*this)) ope rhs;                                                   \
    }
    ENUMERATE_OPS(ENUMERATE_OP)
#undef ENUMERATE_OP
};

template <typename T, unsigned L>
class Vec : public VecBase<Vec<T, L>> {
    Array<T, L> m_elems;

public:
    static constexpr unsigned length = L;

    constexpr Vec() : Vec(T(0)) {}

    // Duplication construction, e.g. Vec3f(1.0f) == Vec3f(1.0f, 1.0f, 1.0f)
    constexpr Vec(T t) requires(L != 1);

    // Construction from all components e.g. Vec2f(1.0f, 2.0f) or Vec3f(1.0f, 2.0f, 3.0f)
    template <typename... Ts>
    constexpr Vec(Ts... ts) requires(sizeof...(Ts) == L) : m_elems{static_cast<T>(ts)...} {}

    // Construction from a smaller vector + remaining scalar components, e.g. Vec4f(Vec3f(), 1.0f)
    template <unsigned L1, typename... Ts>
    constexpr Vec(const Vec<T, L1> &vec, Ts... ts) requires(sizeof...(Ts) == L - L1);

    // Casting construction.
    template <typename T1>
    constexpr Vec(const Vec<T1, L> &vec);

    // Truncating construction from a larger vector, e.g. Vec3f(Vec4f())
    template <unsigned L1>
    constexpr Vec(const Vec<T, L1> &vec) requires(L1 > L);

#define ENUMERATE_OP(op, ope)                                                                                          \
    constexpr Vec &operator ope(T rhs) {                                                                               \
        for (unsigned i = 0; i < L; i++) {                                                                             \
            m_elems[i] ope rhs;                                                                                        \
        }                                                                                                              \
        return *this;                                                                                                  \
    }                                                                                                                  \
    constexpr Vec &operator ope(const Vec &rhs) {                                                                      \
        for (unsigned i = 0; i < L; i++) {                                                                             \
            m_elems[i] ope rhs.m_elems[i];                                                                             \
        }                                                                                                              \
        return *this;                                                                                                  \
    }
    ENUMERATE_OPS(ENUMERATE_OP)
#undef ENUMERATE_OP

    void set_x(T x) requires(L >= 1) { m_elems[0] = x; }
    void set_y(T y) requires(L >= 2) { m_elems[1] = y; }
    void set_z(T z) requires(L >= 3) { m_elems[2] = z; }
    void set_w(T w) requires(L >= 4) { m_elems[3] = w; }

    constexpr T &operator[](unsigned index) { return m_elems[index]; }
    constexpr T operator[](unsigned index) const { return m_elems[index]; }
    constexpr T x() const requires(L >= 1) { return m_elems[0]; }
    constexpr T y() const requires(L >= 2) { return m_elems[1]; }
    constexpr T z() const requires(L >= 3) { return m_elems[2]; }
    constexpr T w() const requires(L >= 4) { return m_elems[3]; }
};
#undef DEFINE_OP

using Vec2f = Vec<float, 2>;
using Vec3f = Vec<float, 3>;
using Vec4f = Vec<float, 4>;
using Vec2i = Vec<int32_t, 2>;
using Vec3i = Vec<int32_t, 3>;
using Vec4i = Vec<int32_t, 4>;
using Vec2u = Vec<uint32_t, 2>;
using Vec3u = Vec<uint32_t, 3>;
using Vec4u = Vec<uint32_t, 4>;

template <typename T, unsigned L>
constexpr Vec<T, L>::Vec(T t) requires(L != 1) {
    for (unsigned i = 0; i < L; i++) {
        m_elems[i] = t;
    }
}

template <typename T, unsigned L>
template <unsigned L1, typename... Ts>
constexpr Vec<T, L>::Vec(const Vec<T, L1> &vec, Ts... ts) requires(sizeof...(Ts) == L - L1) {
    for (unsigned i = 0; i < L1; i++) {
        m_elems[i] = vec[i];
    }
    Array packed{static_cast<T>(ts)...};
    for (unsigned i = 0; i < sizeof...(Ts); i++) {
        m_elems[L1 + i] = packed[i];
    }
}

template <typename T, unsigned L>
template <typename T1>
constexpr Vec<T, L>::Vec(const Vec<T1, L> &vec) {
    for (unsigned i = 0; i < L; i++) {
        m_elems[i] = static_cast<T>(vec[i]);
    }
}

template <typename T, unsigned L>
template <unsigned L1>
constexpr Vec<T, L>::Vec(const Vec<T, L1> &vec) requires(L1 > L) {
    for (unsigned i = 0; i < L; i++) {
        m_elems[i] = vec[i];
    }
}

#define DEFINE_CWISE_BINARY(name)                                                                                      \
    template <typename T, unsigned L>                                                                                  \
    constexpr Vec<T, L> name(const Vec<T, L> &lhs, const Vec<T, L> &rhs) {                                             \
        Vec<T, L> ret;                                                                                                 \
        for (unsigned i = 0; i < L; i++) {                                                                             \
            ret[i] = name(lhs[i], rhs[i]);                                                                             \
        }                                                                                                              \
        return ret;                                                                                                    \
    }
#define DEFINE_CWISE_UNARY(name)                                                                                       \
    template <typename T, unsigned L>                                                                                  \
    constexpr Vec<T, L> name(const Vec<T, L> &vec) {                                                                   \
        Vec<T, L> ret;                                                                                                 \
        for (unsigned i = 0; i < L; i++) {                                                                             \
            ret[i] = name(vec[i]);                                                                                     \
        }                                                                                                              \
        return ret;                                                                                                    \
    }

DEFINE_CWISE_BINARY(min)
DEFINE_CWISE_BINARY(max)
DEFINE_CWISE_BINARY(pow)
DEFINE_CWISE_UNARY(abs)
DEFINE_CWISE_UNARY(ceil)
DEFINE_CWISE_UNARY(floor)
DEFINE_CWISE_UNARY(round)
DEFINE_CWISE_UNARY(sign)
#undef DEFINE_CWISE_BINARY
#undef DEFINE_CWISE_UNARY

template <typename T, unsigned L>
constexpr Vec<T, L> operator-(const Vec<T, L> &vec) {
    Vec<T, L> ret;
    for (unsigned i = 0; i < L; i++) {
        ret[i] = -vec[i];
    }
    return ret;
}

template <typename T>
constexpr Vec<T, 3> cross(const Vec<T, 3> &lhs, const Vec<T, 3> &rhs) {
    return {
        lhs.y() * rhs.z() - lhs.z() * rhs.y(),
        lhs.z() * rhs.x() - lhs.x() * rhs.z(),
        lhs.x() * rhs.y() - lhs.y() * rhs.x(),
    };
}

template <typename T, unsigned L>
constexpr T distance(const Vec<T, L> &lhs, const Vec<T, L> &rhs) {
    return magnitude(rhs - lhs);
}

template <typename T, unsigned L>
constexpr T dot(const Vec<T, L> &lhs, const Vec<T, L> &rhs) {
    T ret = T(0);
    for (unsigned i = 0; i < L; i++) {
        ret += lhs[i] * rhs[i];
    }
    return ret;
}

template <typename T, unsigned L>
constexpr T magnitude(const Vec<T, L> &vec) {
    return sqrt(square_magnitude(vec));
}

template <typename T, unsigned L>
constexpr Vec<T, L> normalise(const Vec<T, L> &vec) {
    return vec / magnitude(vec);
}

template <typename T, unsigned L>
constexpr T square_magnitude(const Vec<T, L> &vec) {
    return dot(vec, vec);
}

} // namespace vull

#undef ENUMERATE_OPS
