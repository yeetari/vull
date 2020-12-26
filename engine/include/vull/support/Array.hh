#pragma once

#include <vull/support/Assert.hh>

#include <cstdint>

template <typename T, std::uint32_t N>
struct Array {
    // NOLINTNEXTLINE
    T m_data[N];

    T &operator[](std::uint32_t index);
    const T &operator[](std::uint32_t index) const;

    T *data() { return static_cast<T *>(m_data); }
    const T *data() const { return static_cast<const T *>(m_data); }

    constexpr std::uint32_t length() const { return N; }
    constexpr std::uint32_t size() const { return N * sizeof(T); }
};

template <typename T, typename... Args>
Array(T, Args...) -> Array<T, sizeof...(Args) + 1>;

template <typename T, std::uint32_t N>
T &Array<T, N>::operator[](std::uint32_t index) {
    ASSERT(index < N);
    return m_data[index];
}

template <typename T, std::uint32_t N>
const T &Array<T, N>::operator[](std::uint32_t index) const {
    ASSERT(index < N);
    return m_data[index];
}
