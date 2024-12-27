#pragma once

#include <vull/maths/common.hh>
#include <vull/support/assert.hh>
#include <vull/support/span.hh>
#include <vull/support/utility.hh>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace vull {

template <typename T, typename SizeT = uint32_t>
class Vector {
    using base_type = remove_ref<T>;
    struct RefWrapper {
        base_type *ptr;
        // When T is a reference T is the same as base_type &.
        operator T() const { return *ptr; }
    };
    using storage_type = conditional<is_ref<T>, RefWrapper, T>;

private:
    storage_type *m_data{nullptr};
    SizeT m_capacity{0};
    SizeT m_size{0};

public:
    constexpr Vector() = default;
    template <typename... Args>
    explicit Vector(SizeT size, Args &&...args);
    template <typename It>
    Vector(It first, It last);
    Vector(const Vector &) = delete;
    Vector(Vector &&);
    ~Vector();

    Vector &operator=(const Vector &) = delete;
    Vector &operator=(Vector &&);

    void clear();
    void ensure_capacity(SizeT capacity);
    template <typename... Args>
    void ensure_size(SizeT size, Args &&...args);
    void reallocate(SizeT capacity);

    template <typename Container>
    void extend(const Container &container);
    template <typename... Args>
    T &emplace(Args &&...args) requires(!is_ref<T>);
    void push(const T &elem) requires(!is_ref<T>);
    void push(T &&elem);
    void pop();

    // TODO: Would it be better to add Span<T &> support?
    Span<storage_type> span() { return {m_data, static_cast<size_t>(m_size)}; }
    Span<const storage_type> span() const { return {m_data, static_cast<size_t>(m_size)}; }
    Span<storage_type> take_all();
    storage_type take_last();

    storage_type *begin() { return m_data; }
    storage_type *end() { return m_data + m_size; }
    const storage_type *begin() const { return m_data; }
    const storage_type *end() const { return m_data + m_size; }

    storage_type &operator[](SizeT index);
    const storage_type &operator[](SizeT index) const;

    storage_type &first() { return begin()[0]; }
    const storage_type &first() const { return begin()[0]; }
    storage_type &last() { return end()[-1]; }
    const storage_type &last() const { return end()[-1]; }

    bool empty() const { return m_size == 0; }
    storage_type *data() const { return m_data; }
    SizeT capacity() const { return m_capacity; }
    SizeT size() const { return m_size; }
    SizeT size_bytes() const { return m_size * sizeof(storage_type); }
};

template <typename T>
using LargeVector = Vector<T, size_t>;

template <typename T, typename SizeT>
template <typename... Args>
Vector<T, SizeT>::Vector(SizeT size, Args &&...args) {
    ensure_size(size, vull::forward<Args>(args)...);
}

template <typename T, typename SizeT>
template <typename It>
Vector<T, SizeT>::Vector(It first, It last) {
    for (; first != last; ++first) {
        emplace(*first);
    }
}

template <typename T, typename SizeT>
Vector<T, SizeT>::Vector(Vector &&other) {
    m_data = vull::exchange(other.m_data, nullptr);
    m_capacity = vull::exchange(other.m_capacity, SizeT(0));
    m_size = vull::exchange(other.m_size, SizeT(0));
}

template <typename T, typename SizeT>
Vector<T, SizeT>::~Vector() {
    clear();
}

template <typename T, typename SizeT>
Vector<T, SizeT> &Vector<T, SizeT>::operator=(Vector &&other) {
    if (this != &other) {
        clear();
        m_data = vull::exchange(other.m_data, nullptr);
        m_capacity = vull::exchange(other.m_capacity, SizeT(0));
        m_size = vull::exchange(other.m_size, SizeT(0));
    }
    return *this;
}

template <typename T, typename SizeT>
void Vector<T, SizeT>::clear() {
    if constexpr (!is_trivially_destructible<storage_type>) {
        for (auto *elem = end(); elem != begin();) {
            (--elem)->~storage_type();
        }
    }
    m_size = 0;
    m_capacity = 0;
    delete[] reinterpret_cast<uint8_t *>(vull::exchange(m_data, nullptr));
}

template <typename T, typename SizeT>
void Vector<T, SizeT>::ensure_capacity(SizeT capacity) {
    if (capacity > m_capacity) {
        reallocate(vull::max(SizeT(m_capacity * 2 + 1), capacity));
    }
}

template <typename T, typename SizeT>
template <typename... Args>
void Vector<T, SizeT>::ensure_size(SizeT size, Args &&...args) {
    if (size <= m_size) {
        return;
    }
    ensure_capacity(size);
    if constexpr (!is_trivially_constructible<T> || sizeof...(Args) != 0) {
        static_assert(!is_ref<T>);
        for (SizeT i = m_size; i < size; i++) {
            new (begin() + i) T(vull::forward<Args>(args)...);
        }
    } else {
        memset(begin() + m_size, 0, size * sizeof(storage_type) - m_size * sizeof(storage_type));
    }
    m_size = size;
}

template <typename T, typename SizeT>
void Vector<T, SizeT>::reallocate(SizeT capacity) {
    VULL_ASSERT(capacity >= m_size);
    auto *new_data = reinterpret_cast<storage_type *>(new uint8_t[capacity * sizeof(storage_type)]);
    if constexpr (!is_trivially_copyable<storage_type>) {
        static_assert(!is_ref<T>);
        for (auto *data = new_data; auto &elem : *this) {
            new (data++) storage_type(vull::move(elem));
        }
        for (auto *elem = end(); elem != begin();) {
            (--elem)->~storage_type();
        }
    } else if (m_size != 0) {
        memcpy(new_data, m_data, size_bytes());
    }
    delete[] reinterpret_cast<uint8_t *>(m_data);
    m_data = new_data;
    m_capacity = capacity;
}

template <typename T, typename SizeT>
template <typename Container>
void Vector<T, SizeT>::extend(const Container &container) {
    if (container.empty()) {
        return;
    }
    ensure_capacity(m_size + static_cast<SizeT>(container.size()));
    if constexpr (is_trivially_copyable<storage_type>) {
        memcpy(end(), container.data(), container.size_bytes());
        m_size += static_cast<SizeT>(container.size());
    } else {
        for (const auto &elem : container) {
            push(elem);
        }
    }
}

template <typename T, typename SizeT>
template <typename... Args>
T &Vector<T, SizeT>::emplace(Args &&...args) requires(!is_ref<T>) {
    ensure_capacity(m_size + 1);
    new (end()) T(vull::forward<Args>(args)...);
    return (*this)[m_size++];
}

template <typename T, typename SizeT>
void Vector<T, SizeT>::push(const T &elem) requires(!is_ref<T>) {
    ensure_capacity(m_size + 1);
    if constexpr (is_trivially_copyable<T>) {
        memcpy(end(), &elem, sizeof(T));
    } else {
        new (end()) storage_type(elem);
    }
    m_size++;
}

template <typename T, typename SizeT>
void Vector<T, SizeT>::push(T &&elem) {
    ensure_capacity(m_size + 1);
    if constexpr (is_ref<T>) {
        new (end()) storage_type{&elem};
    } else {
        new (end()) storage_type(vull::move(elem));
    }
    m_size++;
}

template <typename T, typename SizeT>
void Vector<T, SizeT>::pop() {
    VULL_ASSERT(!empty());
    m_size--;
    end()->~storage_type();
}

template <typename T, typename SizeT>
Span<typename Vector<T, SizeT>::storage_type> Vector<T, SizeT>::take_all() {
    m_capacity = 0;
    return {vull::exchange(m_data, nullptr), vull::exchange(m_size, SizeT(0))};
}

template <typename T, typename SizeT>
typename Vector<T, SizeT>::storage_type Vector<T, SizeT>::take_last() {
    VULL_ASSERT(!empty());
    m_size--;
    auto value = vull::move(*end());
    if constexpr (!is_ref<T>) {
        end()->~T();
        return value;
    } else {
        return value;
    }
}

template <typename T, typename SizeT>
typename Vector<T, SizeT>::storage_type &Vector<T, SizeT>::operator[](SizeT index) {
    VULL_ASSERT(index < m_size);
    return begin()[index];
}

template <typename T, typename SizeT>
const typename Vector<T, SizeT>::storage_type &Vector<T, SizeT>::operator[](SizeT index) const {
    VULL_ASSERT(index < m_size);
    return begin()[index];
}

} // namespace vull
