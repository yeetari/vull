#pragma once

#include <vull/support/Array.hh>
#include <vull/support/Assert.hh>

#include <cstdint>
#include <utility>

template <typename T>
class Optional {
    alignas(T) Array<std::uint8_t, sizeof(T)> m_data{};
    bool m_present{false};

public:
    constexpr Optional() = default;
    constexpr Optional(const T &value) : m_present(true) { new (m_data.data()) T(value); }
    constexpr Optional(T &&value) : m_present(true) { new (m_data.data()) T(std::move(value)); }

    constexpr explicit operator bool() const noexcept { return m_present; }
    constexpr const T &operator*() const {
        ASSERT(m_present);
        return *reinterpret_cast<const T *>(m_data.data());
    }
    constexpr const T *operator->() const {
        ASSERT(m_present);
        return reinterpret_cast<const T *>(m_data.data());
    }
};
