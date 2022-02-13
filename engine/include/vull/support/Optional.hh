#pragma once

#include <vull/support/Array.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Utility.hh>

namespace vull {

template <typename T>
class Optional {
    alignas(T) Array<uint8_t, sizeof(T)> m_data{};
    bool m_present{false};

public:
    constexpr Optional() = default;
    Optional(const Optional &) = delete;
    Optional(Optional &&);
    ~Optional() { clear(); }

    Optional &operator=(const Optional &) = delete;
    Optional &operator=(Optional &&) = delete;

    void clear();
    template <typename... Args>
    T &emplace(Args &&...args);

    explicit operator bool() const { return m_present; }
    bool has_value() const { return m_present; }

    T &operator*();
    T *operator->();
    const T &operator*() const;
    const T *operator->() const;
};

template <typename T>
Optional<T>::Optional(Optional &&other) : m_present(other.m_present) {
    if (other) {
        new (m_data.data()) T(move(*other));
        other.clear();
    }
}

template <typename T>
void Optional<T>::clear() {
    if constexpr (!IsTriviallyDestructible<T>) {
        if (m_present) {
            operator*().~T();
        }
    }
    m_present = false;
}

template <typename T>
template <typename... Args>
T &Optional<T>::emplace(Args &&...args) {
    clear();
    new (m_data.data()) T(forward<Args>(args)...);
    m_present = true;
    return operator*();
}

template <typename T>
T &Optional<T>::operator*() {
    VULL_ASSERT(m_present);
    return *reinterpret_cast<T *>(m_data.data());
}

template <typename T>
T *Optional<T>::operator->() {
    VULL_ASSERT(m_present);
    return reinterpret_cast<T *>(m_data.data());
}

template <typename T>
const T &Optional<T>::operator*() const {
    VULL_ASSERT(m_present);
    return *reinterpret_cast<const T *>(m_data.data());
}

template <typename T>
const T *Optional<T>::operator->() const {
    VULL_ASSERT(m_present);
    return reinterpret_cast<const T *>(m_data.data());
}

} // namespace vull
