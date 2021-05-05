#pragma once

#include <vull/support/Assert.hh>
#include <vull/support/Span.hh>

#include <cstddef>

template <typename T, std::size_t N>
struct Array {
    // NOLINTNEXTLINE
    alignas(T) T m_data[N];

    // Allow implicit conversion from `Array<T>` to `Span<T>`, or from `const Array<T>` to `Span<const T>`.
    constexpr operator Span<T>() { return {data(), size()}; }
    constexpr operator Span<const T>() const { return {data(), size()}; }

    constexpr T *begin() { return data(); }
    constexpr T *end() { return data() + size(); }
    constexpr const T *begin() const { return data(); }
    constexpr const T *end() const { return data() + size(); }

    constexpr T &operator[](std::size_t index);
    constexpr const T &operator[](std::size_t index) const;

    constexpr T *data() { return static_cast<T *>(m_data); }
    constexpr const T *data() const { return static_cast<const T *>(m_data); }
    constexpr std::size_t size() const { return N; }
    constexpr std::size_t size_bytes() const { return N * sizeof(T); }
};

template <typename T, typename... Args>
Array(T, Args...) -> Array<T, sizeof...(Args) + 1>;

template <typename T, std::size_t N>
constexpr T &Array<T, N>::operator[](std::size_t index) {
    ASSERT(index < N);
    return m_data[index];
}

template <typename T, std::size_t N>
constexpr const T &Array<T, N>::operator[](std::size_t index) const {
    ASSERT(index < N);
    return m_data[index];
}
