#pragma once

#include <vull/support/Assert.hh>

#include <concepts>
#include <cstddef>

/// Dynamically sized span.
template <typename T>
class Span {
    T *const m_data;
    const std::size_t m_size;

public:
    constexpr Span(T *data, std::size_t size) : m_data(data), m_size(size) {}

    // Allow implicit conversion from `Span<T>` to `Span<void>`.
    constexpr operator Span<void>() { return {m_data, m_size}; }
    constexpr operator Span<const void>() const { return {m_data, m_size}; }

    constexpr T *begin() const { return m_data; }
    constexpr T *end() const { return m_data + m_size; }

    template <typename U = std::conditional_t<std::same_as<T, void>, char, T>>
    constexpr U &operator[](std::size_t index) const requires(!std::same_as<T, void>) {
        ASSERT(index < m_size);
        return m_data[index];
    }

    constexpr T *data() const { return m_data; }
    constexpr std::size_t size() const { return m_size; }
    constexpr std::size_t size_bytes() const { return m_size * sizeof(T); }
};
