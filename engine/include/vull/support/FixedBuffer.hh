#pragma once

#include <vull/support/Utility.hh>

#include <stddef.h>
#include <stdint.h>

namespace vull {

template <TriviallyCopyable T>
class FixedBuffer {
    T *m_data{nullptr};
    size_t m_size{0};

    FixedBuffer(T *data, size_t size) : m_data(data), m_size(size) {}

public:
    static FixedBuffer create_uninitialised(size_t size);
    static FixedBuffer create_zeroed(size_t size);

    FixedBuffer() = default;
    FixedBuffer(const FixedBuffer &) = delete;
    FixedBuffer(FixedBuffer &&other)
        : m_data(vull::exchange(other.m_data, nullptr)), m_size(vull::exchange(other.m_size, 0u)) {}
    ~FixedBuffer() {
        // TODO: Use sized deallocation.
        delete[] m_data;
    }

    FixedBuffer &operator=(const FixedBuffer &) = delete;
    FixedBuffer &operator=(FixedBuffer &&);

    T *begin() { return m_data; }
    T *end() { return m_data + m_size; }
    const T *begin() const { return m_data; }
    const T *end() const { return m_data + m_size; }

    T &operator[](size_t index) { return m_data[index]; }
    const T &operator[](size_t index) const { return m_data[index]; }

    bool empty() const { return m_size == 0; }
    T *data() { return m_data; }
    const T *data() const { return m_data; }
    size_t size() const { return m_size; }
    size_t size_bytes() const { return m_size * sizeof(T); }
};

template <TriviallyCopyable T>
FixedBuffer<T> &FixedBuffer<T>::operator=(FixedBuffer &&other) {
    FixedBuffer moved(vull::move(other));
    vull::swap(m_data, moved.m_data);
    vull::swap(m_size, moved.m_size);
    return *this;
}

template <TriviallyCopyable T>
FixedBuffer<T> FixedBuffer<T>::create_uninitialised(size_t size) {
    auto *data = new T[size];
    return {data, size};
}

template <TriviallyCopyable T>
FixedBuffer<T> FixedBuffer<T>::create_zeroed(size_t size) {
    auto buffer = create_uninitialised(size);
    __builtin_memset(buffer.data(), 0, size * sizeof(T));
    return buffer;
}

} // namespace vull
