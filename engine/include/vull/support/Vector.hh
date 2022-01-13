#pragma once

#include <vull/maths/Common.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Span.hh>
#include <vull/support/Utility.hh>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace vull {

template <typename T, typename SizeType = uint32_t>
class Vector {
    T *m_data{nullptr};
    SizeType m_capacity{0};
    SizeType m_size{0};

public:
    constexpr Vector() = default;
    template <typename... Args>
    explicit Vector(SizeType size, Args &&...args);
    Vector(const Vector &) = delete;
    Vector(Vector &&);
    ~Vector();

    Vector &operator=(const Vector &) = delete;
    Vector &operator=(Vector &&);

    void clear();
    void ensure_capacity(SizeType capacity);
    template <typename... Args>
    void ensure_size(SizeType size, Args &&...args);
    void reallocate(SizeType capacity);

    template <typename... Args>
    T &emplace(Args &&...args);
    void push(const T &elem);
    void push(T &&elem);
    void pop();

    Span<T> span() { return {m_data, m_size}; }
    Span<const T> span() const { return {m_data, m_size}; }

    T *begin() { return m_data; }
    T *end() { return m_data + m_size; }
    const T *begin() const { return m_data; }
    const T *end() const { return m_data + m_size; }

    T &operator[](SizeType index);
    const T &operator[](SizeType index) const;

    T &first() { return begin()[0]; }
    const T &first() const { return begin()[0]; }
    T &last() { return end()[-1]; }
    const T &last() const { return end()[-1]; }

    bool empty() const { return m_size == 0; }
    T *data() const { return m_data; }
    SizeType capacity() const { return m_capacity; }
    SizeType size() const { return m_size; }
    SizeType size_bytes() const { return m_size * sizeof(T); }
};

template <typename T>
using LargeVector = Vector<T, size_t>;

template <typename T, typename SizeType>
template <typename... Args>
Vector<T, SizeType>::Vector(SizeType size, Args &&...args) {
    ensure_size(size, forward<Args>(args)...);
}

template <typename T, typename SizeType>
Vector<T, SizeType>::Vector(Vector &&other) {
    m_data = exchange(other.m_data, nullptr);
    m_capacity = exchange(other.m_capacity, 0u);
    m_size = exchange(other.m_size, 0u);
}

template <typename T, typename SizeType>
Vector<T, SizeType>::~Vector() {
    clear();
    delete[] reinterpret_cast<uint8_t *>(m_data);
}

template <typename T, typename SizeType>
Vector<T, SizeType> &Vector<T, SizeType>::operator=(Vector &&other) {
    if (this != &other) {
        clear();
        m_data = exchange(other.m_data, nullptr);
        m_capacity = exchange(other.m_capacity, 0u);
        m_size = exchange(other.m_size, 0u);
    }
    return *this;
}

template <typename T, typename SizeType>
void Vector<T, SizeType>::clear() {
    if constexpr (!IsTriviallyDestructible<T>) {
        for (auto *elem = end(); elem != begin();) {
            (--elem)->~T();
        }
    }
    m_size = 0;
}

template <typename T, typename SizeType>
void Vector<T, SizeType>::ensure_capacity(SizeType capacity) {
    if (capacity > m_capacity) {
        reallocate(max(m_capacity * 2 + 1, capacity));
    }
}

template <typename T, typename SizeType>
template <typename... Args>
void Vector<T, SizeType>::ensure_size(SizeType size, Args &&...args) {
    if (size <= m_size) {
        return;
    }
    ensure_capacity(size);
    if constexpr (!IsTriviallyCopyable<T> || sizeof...(Args) != 0) {
        for (SizeType i = m_size; i < size; i++) {
            new (begin() + i) T(forward<Args>(args)...);
        }
    } else {
        memset(begin() + m_size, 0, size * sizeof(T) - m_size * sizeof(T));
    }
    m_size = size;
}

template <typename T, typename SizeType>
void Vector<T, SizeType>::reallocate(SizeType capacity) {
    VULL_ASSERT(capacity >= m_size);
    T *new_data = reinterpret_cast<T *>(new uint8_t[capacity * sizeof(T)]);
    if constexpr (!IsTriviallyCopyable<T>) {
        for (auto *data = new_data; auto &elem : *this) {
            new (data++) T(move(elem));
        }
        for (auto *elem = end(); elem != begin();) {
            (--elem)->~T();
        }
    } else if (m_size != 0) {
        memcpy(new_data, m_data, size_bytes());
    }
    delete[] reinterpret_cast<uint8_t *>(m_data);
    m_data = new_data;
    m_capacity = capacity;
}

template <typename T, typename SizeType>
template <typename... Args>
T &Vector<T, SizeType>::emplace(Args &&...args) {
    ensure_capacity(m_size + 1);
    new (end()) T(forward<Args>(args)...);
    return (*this)[m_size++];
}

template <typename T, typename SizeType>
void Vector<T, SizeType>::push(const T &elem) {
    ensure_capacity(m_size + 1);
    if constexpr (IsTriviallyCopyable<T>) {
        memcpy(end(), &elem, sizeof(T));
    } else {
        new (end()) T(elem);
    }
    m_size++;
}

template <typename T, typename SizeType>
void Vector<T, SizeType>::push(T &&elem) {
    ensure_capacity(m_size + 1);
    new (end()) T(move(elem));
    m_size++;
}

template <typename T, typename SizeType>
void Vector<T, SizeType>::pop() {
    VULL_ASSERT(!empty());
    m_size--;
    end()->~T();
}

template <typename T, typename SizeType>
T &Vector<T, SizeType>::operator[](SizeType index) {
    VULL_ASSERT(index < m_size);
    return begin()[index];
}

template <typename T, typename SizeType>
const T &Vector<T, SizeType>::operator[](SizeType index) const {
    VULL_ASSERT(index < m_size);
    return begin()[index];
}

} // namespace vull
