#pragma once

#include <cstdint>

template <typename T, std::uint32_t N>
struct Array {
    // NOLINTNEXTLINE
    T m_data[N];

    T *data() { return static_cast<T *>(m_data); }
    const T *data() const { return static_cast<const T *>(m_data); }

    constexpr std::uint32_t length() const { return N; }
    constexpr std::uint32_t size() const { return N * sizeof(T); }
};

template <typename T, typename... Args>
Array(T, Args...) -> Array<T, sizeof...(Args) + 1>;
