#pragma once

#include <vull/container/Array.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Utility.hh>

namespace vull {

template <typename T>
class Optional {
    AlignedStorage<T> m_storage{};
    bool m_present{false};

public:
    constexpr Optional() = default;
    Optional(const T &value) : m_present(true) { m_storage.set(value); }
    Optional(T &&value) : m_present(true) { m_storage.set(move(value)); }
    // clang-format off
    Optional(const Optional &) requires is_trivially_copyable<T> = default;
    // clang-format on
    Optional(const Optional &);
    Optional(Optional &&);
    ~Optional() { clear(); }

    Optional &operator=(const Optional &);
    Optional &operator=(Optional &&);

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
class Optional<T &> {
    T *m_ptr{nullptr};

public:
    constexpr Optional() = default;
    Optional(T &ref) : m_ptr(&ref) {}
    Optional(const Optional &) = default;
    Optional(Optional &&other) : m_ptr(exchange(other.m_ptr, nullptr)) {}
    ~Optional() = default;

    Optional &operator=(const Optional &) = default;
    Optional &operator=(Optional &&);

    T value_or(T fallback) const;

    explicit operator bool() const { return m_ptr != nullptr; }
    bool has_value() const { return m_ptr != nullptr; }

    T *ptr() const { return m_ptr; }

    T &operator*();
    T *operator->();
    const T &operator*() const;
    const T *operator->() const;
};

template <typename T>
Optional<T>::Optional(const Optional &other) : m_present(other.m_present) {
    if (other) {
        m_storage.set(*other);
    }
}

template <typename T>
Optional<T>::Optional(Optional &&other) : m_present(other.m_present) {
    if (other) {
        m_storage.set(move(*other));
        other.clear();
    }
}

template <typename T>
Optional<T> &Optional<T>::operator=(const Optional &other) {
    if (this != &other) {
        clear();
        m_present = other.m_present;
        if (other) {
            m_storage.set(*other);
        }
    }
    return *this;
}

template <typename T>
Optional<T> &Optional<T>::operator=(Optional &&other) {
    if (this != &other) {
        clear();
        m_present = other.m_present;
        if (other) {
            m_storage.set(move(*other));
            other.clear();
        }
    }
    return *this;
}

template <typename T>
void Optional<T>::clear() {
    if constexpr (!is_trivially_destructible<T>) {
        if (m_present) {
            m_storage.release();
        }
    }
    m_present = false;
}

template <typename T>
template <typename... Args>
T &Optional<T>::emplace(Args &&...args) {
    clear();
    m_storage.emplace(forward<Args>(args)...);
    m_present = true;
    return operator*();
}

template <typename T>
T &Optional<T>::operator*() {
    VULL_ASSERT(m_present);
    return m_storage.get();
}

template <typename T>
T *Optional<T>::operator->() {
    VULL_ASSERT(m_present);
    return &m_storage.get();
}

template <typename T>
const T &Optional<T>::operator*() const {
    VULL_ASSERT(m_present);
    return m_storage.get();
}

template <typename T>
const T *Optional<T>::operator->() const {
    VULL_ASSERT(m_present);
    return &m_storage.get();
}

template <typename T>
Optional<T &> &Optional<T &>::operator=(Optional &&other) {
    m_ptr = exchange(other.m_ptr, nullptr);
    return *this;
}

template <typename T>
T Optional<T &>::value_or(T fallback) const {
    return m_ptr != nullptr ? *m_ptr : fallback;
}

template <typename T>
T &Optional<T &>::operator*() {
    VULL_ASSERT(m_ptr != nullptr);
    return *m_ptr;
}

template <typename T>
T *Optional<T &>::operator->() {
    VULL_ASSERT(m_ptr != nullptr);
    return m_ptr;
}

template <typename T>
const T &Optional<T &>::operator*() const {
    VULL_ASSERT(m_ptr != nullptr);
    return *m_ptr;
}

template <typename T>
const T *Optional<T &>::operator->() const {
    VULL_ASSERT(m_ptr != nullptr);
    return m_ptr;
}

} // namespace vull
