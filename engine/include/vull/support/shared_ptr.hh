#pragma once

#include <vull/support/assert.hh>
#include <vull/support/utility.hh>

namespace vull {

template <typename T>
class SharedPtr {
    template <typename>
    friend class SharedPtr;

private:
    T *m_ptr{nullptr};

public:
    SharedPtr() = default;
    ~SharedPtr();

    // Construction from pointer.
    explicit SharedPtr(T *ptr);
    template <typename U>
    explicit SharedPtr(U *ptr) requires vull::is_convertible_to<U *, T *> : SharedPtr(static_cast<T *>(ptr)) {}

    // Copy construction.
    SharedPtr(const SharedPtr &other) : SharedPtr(other.m_ptr) {}
    template <typename U>
    SharedPtr(const SharedPtr<U> &other) requires vull::is_convertible_to<U *, T *>
        : SharedPtr(static_cast<T *>(other.m_ptr)) {}

    // Move construction.
    SharedPtr(SharedPtr &&other) : m_ptr(other.disown()) {}
    template <typename U>
    SharedPtr(SharedPtr<U> &&other) requires vull::is_convertible_to<U *, T *>
        : m_ptr(static_cast<T *>(other.disown())) {}

    // Copy assignment.
    SharedPtr &operator=(const SharedPtr &);
    template <typename U>
    SharedPtr &operator=(const SharedPtr<U> &) requires vull::is_convertible_to<U *, T *>;

    // Move assignment.
    SharedPtr &operator=(SharedPtr &&);
    template <typename U>
    SharedPtr &operator=(SharedPtr<U> &&) requires vull::is_convertible_to<U *, T *>;

    void clear();
    T *disown();
    void swap(SharedPtr &other);

    explicit operator bool() const { return m_ptr != nullptr; }
    bool is_null() const { return m_ptr == nullptr; }

    T &operator*() const;
    T *operator->() const;
    T *ptr() const { return m_ptr; }
};

template <typename T>
SharedPtr<T>::SharedPtr(T *ptr) : m_ptr(ptr) {
    if (ptr != nullptr) {
        ptr->add_ref();
    }
}

template <typename T>
SharedPtr<T>::~SharedPtr() {
    if (m_ptr != nullptr) {
        m_ptr->sub_ref();
    }
#ifndef NDEBUG
    m_ptr = nullptr;
#endif
}

template <typename T>
SharedPtr<T> &SharedPtr<T>::operator=(const SharedPtr &other) {
    SharedPtr(other).swap(*this);
    return *this;
}

template <typename T>
template <typename U>
SharedPtr<T> &SharedPtr<T>::operator=(const SharedPtr<U> &other) requires vull::is_convertible_to<U *, T *> {
    SharedPtr(other).swap(*this);
    return *this;
}

template <typename T>
SharedPtr<T> &SharedPtr<T>::operator=(SharedPtr &&other) {
    SharedPtr(vull::move(other)).swap(*this);
    return *this;
}

template <typename T>
template <typename U>
SharedPtr<T> &SharedPtr<T>::operator=(SharedPtr<U> &&other) requires vull::is_convertible_to<U *, T *> {
    SharedPtr(vull::move(other)).swap(*this);
    return *this;
}

template <typename T>
void SharedPtr<T>::clear() {
    SharedPtr().swap(*this);
}

template <typename T>
T *SharedPtr<T>::disown() {
    return vull::exchange(m_ptr, nullptr);
}

template <typename T>
void SharedPtr<T>::swap(SharedPtr<T> &other) {
    vull::swap(m_ptr, other.m_ptr);
}

template <typename T>
T &SharedPtr<T>::operator*() const {
    VULL_ASSERT(m_ptr != nullptr);
    return *m_ptr;
}

template <typename T>
T *SharedPtr<T>::operator->() const {
    VULL_ASSERT(m_ptr != nullptr);
    return m_ptr;
}

} // namespace vull
