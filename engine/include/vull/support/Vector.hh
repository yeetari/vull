#pragma once

#include <vull/support/Assert.hh>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <utility>

/// Replacement of std::vector with support for inline stack contents and smaller size types. The former allows for much
/// better performance when the vector has a known nominal size more often than not. The latter allows for the vector
/// type itself to be smaller, and removes the need for casting when passing vector size to vulkan.
// TODO: Actually implement inline stack contents.
template <typename T, typename SizeType = std::uint32_t>
class Vector {
    T *m_data{nullptr};
    SizeType m_capacity{0};
    SizeType m_size{0};

public:
    constexpr Vector() = default;
    explicit Vector(SizeType size);
    Vector(const Vector &) = delete;
    Vector(Vector &&other) noexcept
        : m_data(std::exchange(other.m_data, nullptr)), m_capacity(std::exchange(other.m_capacity, 0)),
          m_size(std::exchange(other.m_size, 0)) {}
    ~Vector();

    Vector &operator=(const Vector &) = delete;
    Vector &operator=(Vector &&) = delete;

    T *begin() { return m_data; }
    T *end() { return m_data + m_size; }
    const T *begin() const { return m_data; }
    const T *end() const { return m_data + m_size; }

    void ensure_capacity(SizeType capacity);
    void reallocate(SizeType capacity);
    void resize(SizeType size);

    void push(const T &elem);
    void push(T &&elem);
    std::conditional_t<std::is_trivially_copyable_v<T>, T, void> pop();

    bool empty() const { return m_size == 0; }
    T &operator[](SizeType index);
    const T &operator[](SizeType index) const;

    T *data() const { return m_data; }
    SizeType capacity() const { return m_capacity; }
    SizeType size() const { return m_size; }
    SizeType size_bytes() const { return m_size * sizeof(T); }
};

template <typename T, typename SizeType>
Vector<T, SizeType>::Vector(SizeType size) {
    resize(size);
}

template <typename T, typename SizeType>
Vector<T, SizeType>::~Vector() {
    if constexpr (!std::is_trivially_copyable_v<T>) {
        for (auto *elem = end(); elem != begin();) {
            --elem;
            elem->~T();
        }
    }
    std::free(m_data);
}

template <typename T, typename SizeType>
void Vector<T, SizeType>::ensure_capacity(SizeType capacity) {
    ASSERT(capacity < std::numeric_limits<SizeType>::max());
    if (capacity > m_capacity) {
        reallocate(capacity * 2);
    }
}

template <typename T, typename SizeType>
void Vector<T, SizeType>::reallocate(SizeType capacity) {
    ASSERT(capacity >= m_size);
    T *new_data;
    if constexpr (!std::is_trivially_copyable_v<T>) {
        new_data = static_cast<T *>(std::malloc(capacity * sizeof(T)));
        for (auto &elem : *this) {
            new (new_data++) T(std::move(elem));
        }
        for (auto *elem = end(); elem != begin();) {
            --elem;
            elem->~T();
        }
        std::free(m_data);
    } else {
        new_data = static_cast<T *>(std::realloc(m_data, capacity * sizeof(T)));
    }
    m_data = new_data;
    m_capacity = capacity;
}

template <typename T, typename SizeType>
void Vector<T, SizeType>::resize(SizeType size) {
    ensure_capacity(m_size = size);
}

template <typename T, typename SizeType>
void Vector<T, SizeType>::push(const T &elem) {
    ensure_capacity(m_size + 1);
    if constexpr (std::is_trivially_copyable_v<T>) {
        std::memcpy(end(), &elem, sizeof(T));
    } else {
        new (end()) T(elem);
    }
    ++m_size;
}

template <typename T, typename SizeType>
void Vector<T, SizeType>::push(T &&elem) {
    ensure_capacity(m_size + 1);
    new (end()) T(std::move(elem));
    ++m_size;
}

template <typename T, typename SizeType>
std::conditional_t<std::is_trivially_copyable_v<T>, T, void> Vector<T, SizeType>::pop() {
    ASSERT(!empty());
    auto *elem = end() - 1;
    --m_size;
    if constexpr (!std::is_trivially_copyable_v<T>) {
        elem->~T();
    } else {
        return *elem;
    }
}

template <typename T, typename SizeType>
T &Vector<T, SizeType>::operator[](SizeType index) {
    ASSERT(index < m_size);
    return begin()[index];
}

template <typename T, typename SizeType>
const T &Vector<T, SizeType>::operator[](SizeType index) const {
    ASSERT(index < m_size);
    return begin()[index];
}
