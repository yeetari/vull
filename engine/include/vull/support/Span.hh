#pragma once

#include <vull/support/Array.hh>
#include <vull/support/Assert.hh>

#include <cstdint>
#include <memory>

/// Dynamically sized span.
template <typename T, typename SizeType = std::uint32_t>
class Span {
    T *const m_data;
    const SizeType m_size;

public:
    template <SizeType N>
    constexpr Span(Array<T, N> &array) : m_data(array.data()), m_size(array.size()) {}
    template <typename BeginIt, typename EndIt>
    constexpr Span(BeginIt begin, EndIt end) : m_data(std::to_address(begin)), m_size(end - begin) {}
    constexpr Span(T *data, SizeType size) : m_data(data), m_size(size) {}
    constexpr ~Span() = default;

    // Disallow copying of Span to preserve const-correctness of underlying data - a span must now be passed by
    // reference, with the constness of it specifying the constness of underlying data.
    Span(const Span &) = delete;
    Span(Span &&) = delete;

    Span &operator=(const Span &) = delete;
    Span &operator=(Span &&) = delete;

    T *begin() { return m_data; }
    T *end() { return m_data + m_size; }
    const T *begin() const { return m_data; }
    const T *end() const { return m_data + m_size; }

    T &operator[](std::uint32_t index);
    const T &operator[](std::uint32_t index) const;

    T *data() { return m_data; }
    const T *data() const { return m_data; }
    SizeType size() const { return m_size; }
    SizeType size_bytes() const { return m_size * sizeof(T); }
};

template <typename T, typename SizeType>
T &Span<T, SizeType>::operator[](std::uint32_t index) {
    ASSERT(index < m_size);
    return m_data[index];
}

template <typename T, typename SizeType>
const T &Span<T, SizeType>::operator[](std::uint32_t index) const {
    ASSERT(index < m_size);
    return m_data[index];
}
