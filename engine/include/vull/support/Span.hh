#pragma once

#include <vull/support/Utility.hh>

#include <stddef.h>
#include <stdint.h>

namespace vull {

template <typename T, typename SizeType = uint32_t>
class Span {
    T *m_data{nullptr};
    SizeType m_size{0};

    static constexpr bool is_void = IsSame<RemoveCv<T>, void>;
    using no_void_t = Conditional<is_void, char, T>;

public:
    constexpr Span() = default;
    constexpr Span(T *data, SizeType size) : m_data(data), m_size(size) {}
    constexpr Span(no_void_t &object) requires(!is_void) : m_data(&object), m_size(1) {}

    template <typename U>
    constexpr Span<U, SizeType> as() {
        return {static_cast<U *>(m_data), m_size};
    }

    // Allow implicit conversion from `Span<T>` to `Span<void>`.
    constexpr operator Span<void, SizeType>() { return {data(), size_bytes()}; }
    constexpr operator Span<const void, SizeType>() const { return {data(), size_bytes()}; }

    constexpr T *begin() const { return m_data; }
    constexpr T *end() const { return m_data + m_size; }

    template <typename U = no_void_t>
    constexpr U &operator[](SizeType index) const requires(!is_void) {
        return begin()[index];
    }

    constexpr bool empty() const { return m_size == 0; }
    constexpr T *data() const { return m_data; }
    constexpr SizeType size() const { return m_size; }
    constexpr SizeType size_bytes() const { return m_size * sizeof(T); }
};

template <typename T>
using LargeSpan = Span<T, size_t>;

} // namespace vull
