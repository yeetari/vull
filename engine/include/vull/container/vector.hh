#pragma once

#include <vull/maths/common.hh>
#include <vull/support/assert.hh>
#include <vull/support/span.hh>
#include <vull/support/utility.hh>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace vull {

template <typename T, typename SizeType = uint32_t>
class Vector {
    using BaseType = remove_ref<T>;
    struct RefWrapper {
        BaseType *ptr;
        // When T is a reference T is the same as BaseType &.
        operator T() const { return *ptr; }
    };
    using StorageType = conditional<is_ref<T>, RefWrapper, T>;

    StorageType *m_data{nullptr};
    SizeType m_capacity{0};
    SizeType m_size{0};

public:
    constexpr Vector() = default;
    template <typename... Args>
    explicit Vector(SizeType size, Args &&...args);
    template <typename It>
    Vector(It first, It last);
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
    T &emplace(Args &&...args) requires(!is_ref<T>);
    template <typename Container>
    void extend(const Container &container);
    void push(const T &elem) requires(!is_ref<T>);
    void push(T &&elem);
    void pop();

    // TODO: Would it be better to add Span<T &> support?
    Span<StorageType> span() { return {m_data, static_cast<size_t>(m_size)}; }
    Span<const StorageType> span() const { return {m_data, static_cast<size_t>(m_size)}; }
    Span<StorageType> take_all();
    StorageType take_last();

    StorageType *begin() { return m_data; }
    StorageType *end() { return m_data + m_size; }
    const StorageType *begin() const { return m_data; }
    const StorageType *end() const { return m_data + m_size; }

    StorageType &operator[](SizeType index);
    const StorageType &operator[](SizeType index) const;

    StorageType &first() { return begin()[0]; }
    const StorageType &first() const { return begin()[0]; }
    StorageType &last() { return end()[-1]; }
    const StorageType &last() const { return end()[-1]; }

    bool empty() const { return m_size == 0; }
    StorageType *data() const { return m_data; }
    SizeType capacity() const { return m_capacity; }
    SizeType size() const { return m_size; }
    SizeType size_bytes() const { return m_size * sizeof(StorageType); }
};

template <typename T>
using LargeVector = Vector<T, size_t>;

template <typename T, typename SizeType>
template <typename... Args>
Vector<T, SizeType>::Vector(SizeType size, Args &&...args) {
    ensure_size(size, forward<Args>(args)...);
}

template <typename T, typename SizeType>
template <typename It>
Vector<T, SizeType>::Vector(It first, It last) {
    for (; first != last; ++first) {
        emplace(*first);
    }
}

template <typename T, typename SizeType>
Vector<T, SizeType>::Vector(Vector &&other) {
    m_data = exchange(other.m_data, nullptr);
    m_capacity = exchange(other.m_capacity, SizeType(0));
    m_size = exchange(other.m_size, SizeType(0));
}

template <typename T, typename SizeType>
Vector<T, SizeType>::~Vector() {
    clear();
}

template <typename T, typename SizeType>
Vector<T, SizeType> &Vector<T, SizeType>::operator=(Vector &&other) {
    if (this != &other) {
        clear();
        m_data = exchange(other.m_data, nullptr);
        m_capacity = exchange(other.m_capacity, SizeType(0));
        m_size = exchange(other.m_size, SizeType(0));
    }
    return *this;
}

template <typename T, typename SizeType>
void Vector<T, SizeType>::clear() {
    if constexpr (!is_trivially_destructible<StorageType>) {
        for (auto *elem = end(); elem != begin();) {
            (--elem)->~StorageType();
        }
    }
    m_size = 0;
    m_capacity = 0;
    delete[] reinterpret_cast<uint8_t *>(exchange(m_data, nullptr));
}

template <typename T, typename SizeType>
void Vector<T, SizeType>::ensure_capacity(SizeType capacity) {
    if (capacity > m_capacity) {
        reallocate(max(SizeType(m_capacity * 2 + 1), capacity));
    }
}

template <typename T, typename SizeType>
template <typename... Args>
void Vector<T, SizeType>::ensure_size(SizeType size, Args &&...args) {
    if (size <= m_size) {
        return;
    }
    ensure_capacity(size);
    if constexpr (!is_trivially_constructible<T> || sizeof...(Args) != 0) {
        static_assert(!is_ref<T>);
        for (SizeType i = m_size; i < size; i++) {
            new (begin() + i) T(forward<Args>(args)...);
        }
    } else {
        memset(begin() + m_size, 0, size * sizeof(StorageType) - m_size * sizeof(StorageType));
    }
    m_size = size;
}

template <typename T, typename SizeType>
void Vector<T, SizeType>::reallocate(SizeType capacity) {
    VULL_ASSERT(capacity >= m_size);
    auto *new_data = reinterpret_cast<StorageType *>(new uint8_t[capacity * sizeof(StorageType)]);
    if constexpr (!is_trivially_copyable<StorageType>) {
        static_assert(!is_ref<T>);
        for (auto *data = new_data; auto &elem : *this) {
            new (data++) StorageType(move(elem));
        }
        for (auto *elem = end(); elem != begin();) {
            (--elem)->~StorageType();
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
T &Vector<T, SizeType>::emplace(Args &&...args) requires(!is_ref<T>) {
    ensure_capacity(m_size + 1);
    new (end()) T(forward<Args>(args)...);
    return (*this)[m_size++];
}

template <typename T, typename SizeType>
template <typename Container>
void Vector<T, SizeType>::extend(const Container &container) {
    if (container.empty()) {
        return;
    }
    ensure_capacity(m_size + static_cast<SizeType>(container.size()));
    if constexpr (is_trivially_copyable<StorageType>) {
        memcpy(end(), container.data(), container.size_bytes());
        m_size += static_cast<SizeType>(container.size());
    } else {
        for (const auto &elem : container) {
            push(elem);
        }
    }
}

template <typename T, typename SizeType>
void Vector<T, SizeType>::push(const T &elem) requires(!is_ref<T>) {
    ensure_capacity(m_size + 1);
    if constexpr (is_trivially_copyable<T>) {
        memcpy(end(), &elem, sizeof(T));
    } else {
        new (end()) StorageType(elem);
    }
    m_size++;
}

template <typename T, typename SizeType>
void Vector<T, SizeType>::push(T &&elem) {
    ensure_capacity(m_size + 1);
    if constexpr (is_ref<T>) {
        new (end()) StorageType{&elem};
    } else {
        new (end()) StorageType(move(elem));
    }
    m_size++;
}

template <typename T, typename SizeType>
void Vector<T, SizeType>::pop() {
    VULL_ASSERT(!empty());
    m_size--;
    end()->~StorageType();
}

template <typename T, typename SizeType>
Span<typename Vector<T, SizeType>::StorageType> Vector<T, SizeType>::take_all() {
    m_capacity = 0;
    return {exchange(m_data, nullptr), exchange(m_size, SizeType(0))};
}

template <typename T, typename SizeType>
typename Vector<T, SizeType>::StorageType Vector<T, SizeType>::take_last() {
    VULL_ASSERT(!empty());
    m_size--;
    auto value = move(*end());
    if constexpr (!is_ref<T>) {
        end()->~T();
        return value;
    } else {
        return value;
    }
}

template <typename T, typename SizeType>
typename Vector<T, SizeType>::StorageType &Vector<T, SizeType>::operator[](SizeType index) {
    VULL_ASSERT(index < m_size);
    return begin()[index];
}

template <typename T, typename SizeType>
const typename Vector<T, SizeType>::StorageType &Vector<T, SizeType>::operator[](SizeType index) const {
    VULL_ASSERT(index < m_size);
    return begin()[index];
}

} // namespace vull
