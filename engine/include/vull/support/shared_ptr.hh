#pragma once

#include <vull/support/assert.hh>
#include <vull/support/utility.hh>

namespace vull {

template <typename T>
class SharedPtr {
    T *m_ptr{nullptr};

public:
    SharedPtr() = default;
    explicit SharedPtr(T *ptr);
    SharedPtr(const SharedPtr &other) : SharedPtr(other.m_ptr) {}
    SharedPtr(SharedPtr &&other) : m_ptr(other.disown()) {}
    ~SharedPtr();

    SharedPtr &operator=(const SharedPtr &) = delete;
    SharedPtr &operator=(SharedPtr &&);

    void clear();
    T *disown();

    explicit operator bool() const { return m_ptr != nullptr; }
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
}

template <typename T>
SharedPtr<T> &SharedPtr<T>::operator=(SharedPtr &&other) {
    SharedPtr moved(vull::move(other));
    vull::swap(m_ptr, moved.m_ptr);
    VULL_ASSERT(m_ptr != nullptr);
    return *this;
}

template <typename T>
void SharedPtr<T>::clear() {
    if (m_ptr != nullptr) {
        m_ptr->sub_ref();
    }
    m_ptr = nullptr;
}

template <typename T>
T *SharedPtr<T>::disown() {
    return vull::exchange(m_ptr, nullptr);
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
