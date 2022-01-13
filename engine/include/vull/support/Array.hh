#pragma once

#include <vull/support/Assert.hh>
#include <vull/support/Span.hh>

namespace vull {

template <typename T, auto N>
struct Array {
    // NOLINTNEXTLINE
    alignas(T) T m_data[N];

    constexpr Span<T> span() { return {data(), size()}; }
    constexpr Span<const T> span() const { return {data(), size()}; }

    constexpr T *begin() { return data(); }
    constexpr T *end() { return data() + size(); }
    constexpr const T *begin() const { return data(); }
    constexpr const T *end() const { return data() + size(); }

    constexpr T &operator[](auto index);
    constexpr const T &operator[](auto index) const;

    constexpr T &first() { return begin()[0]; }
    constexpr const T &first() const { return begin()[0]; }
    constexpr T &last() { return end()[-1]; }
    constexpr const T &last() const { return end()[-1]; }

    constexpr T *data() { return static_cast<T *>(m_data); }
    constexpr const T *data() const { return static_cast<const T *>(m_data); }
    constexpr auto size() const { return N; }
    constexpr auto size_bytes() const { return N * sizeof(T); }
};

template <typename T, typename... Args>
Array(T, Args...) -> Array<T, sizeof...(Args) + 1>;

template <typename T, auto N>
constexpr T &Array<T, N>::operator[](auto index) {
    VULL_ASSERT(index < N);
    return begin()[index];
}

template <typename T, auto N>
constexpr const T &Array<T, N>::operator[](auto index) const {
    VULL_ASSERT(index < N);
    return begin()[index];
}

} // namespace vull
