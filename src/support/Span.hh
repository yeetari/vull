#pragma once

#include <cstdint>

/// Dynamically sized span.
template <typename T, typename SizeType = std::uint32_t>
class Span {
    T *const m_data;
    const SizeType m_length;

public:
    constexpr Span(T *data, SizeType length) : m_data(data), m_length(length) {}

    T *data() const { return m_data; }
    SizeType length() const { return m_length; }
    SizeType size() const { return m_length * sizeof(T); }
};
