#pragma once

#include <stddef.h>
#include <stdint.h>

namespace vull {

template <typename T, typename SizeType = uint32_t>
class Span {
    T *m_data{nullptr};
    SizeType m_size{0};

public:
    constexpr Span() = default;
    constexpr Span(T *data, SizeType size) : m_data(data), m_size(size) {}

    // Allow implicit conversion from `Span<T>` to `Span<void>`.
    constexpr operator Span<void>() { return {data(), size_bytes()}; }
    constexpr operator Span<const void>() const { return {data(), size_bytes()}; }

    constexpr T *begin() const { return m_data; }
    constexpr T *end() const { return m_data + m_size; }

    constexpr T *data() const { return m_data; }
    constexpr SizeType size() const { return m_size; }
    constexpr SizeType size_bytes() const { return m_size * sizeof(T); }
};

template <typename T>
using LargeSpan = Span<T, size_t>;

} // namespace vull
