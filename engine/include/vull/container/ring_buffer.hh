#pragma once

#include <vull/container/array.hh>
#include <vull/support/utility.hh>

#include <stdint.h>

namespace vull {

template <typename T, typename SizeType = uint32_t>
class RingBuffer {
    T *const m_data{nullptr};
    const SizeType m_size{0};
    SizeType m_head{0};

public:
    explicit RingBuffer(SizeType size);
    RingBuffer(const RingBuffer &) = delete;
    RingBuffer(RingBuffer &&) = delete;
    ~RingBuffer();

    RingBuffer &operator=(const RingBuffer &) = delete;
    RingBuffer &operator=(RingBuffer &&) = delete;

    void enqueue(T &&elem);
    template <typename... Args>
    T &emplace(Args &&...args);

    T *begin() { return m_data; }
    T *end() { return m_data + m_size; }
    const T *begin() const { return m_data; }
    const T *end() const { return m_data + m_size; }

    T &operator[](SizeType index) { return m_data[(m_head + index) % m_size]; }
    const T &operator[](SizeType index) const { return m_data[(m_head + index) % m_size]; }
    SizeType size() const { return m_size; }
};

template <typename T, typename SizeType>
RingBuffer<T, SizeType>::RingBuffer(SizeType size)
    : m_data(reinterpret_cast<T *>(new uint8_t[size * sizeof(T)])), m_size(size) {
    for (SizeType i = 0; i < size; i++) {
        new (&m_data[i]) T;
    }
}

template <typename T, typename SizeType>
RingBuffer<T, SizeType>::~RingBuffer() {
    for (SizeType i = 0; i < m_size; i++) {
        m_data[i].~T();
    }
    delete[] reinterpret_cast<uint8_t *>(m_data);
}

template <typename T, typename SizeType>
void RingBuffer<T, SizeType>::enqueue(T &&elem) {
    auto &slot = m_data[(m_head + m_size) % m_size];
    slot.~T();
    slot = forward<T>(elem);
    m_head = (m_head + 1) % m_size;
}

template <typename T, typename SizeType>
template <typename... Args>
T &RingBuffer<T, SizeType>::emplace(Args &&...args) {
    auto &slot = m_data[(m_head + m_size) % m_size];
    slot.~T();
    new (&slot) T(forward<Args>(args)...);
    m_head = (m_head + 1) % m_size;
    return slot;
}

} // namespace vull
