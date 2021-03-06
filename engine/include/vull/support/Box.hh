#pragma once

#include <vull/support/Assert.hh>

#include <concepts>
#include <utility>

template <typename T>
class Box { // NOLINT
    T *m_data;

public:
    template <typename... Args>
    static Box create(Args &&...args) {
        // NOLINTNEXTLINE
        return Box(new T(std::forward<Args>(args)...));
    }

    constexpr Box() : m_data(nullptr) {}
    constexpr explicit Box(T *data) : m_data(data) {}
    Box(const Box &) = delete;
    template <typename U>
    Box(Box<U> &&other) noexcept requires std::derived_from<U, T> : m_data(std::exchange(other.data(), nullptr)) {}
    ~Box() { delete m_data; }

    Box &operator=(const Box &) = delete;
    Box &operator=(T *data) {
        delete m_data;
        m_data = data;
        return *this;
    }

    template <typename U>
    Box &operator=(Box<U> &&other) noexcept requires std::derived_from<U, T> {
        *this = std::exchange(other.data(), nullptr);
        return *this;
    }

    T *operator*() const { return m_data; }
    T *operator->() const {
        ASSERT(m_data != nullptr);
        return m_data;
    }
    explicit operator bool() const { return m_data != nullptr; }

    T *&data() { return m_data; }
};
