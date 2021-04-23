#pragma once

#include <vull/support/Assert.hh>
#include <vull/support/Vector.hh>

#include <cstdint>
#include <cstdlib>

template <typename SizeType = std::uint32_t>
class ComponentStorage {
    const SizeType m_element_size;
    void *m_data{nullptr};
    SizeType m_capacity{0};
    Vector<bool, SizeType> m_use_list;

    void reallocate(SizeType capacity) {
        m_data = std::realloc(m_data, capacity * m_element_size);
        m_capacity = capacity;
    }

public:
    constexpr explicit ComponentStorage(SizeType element_size) : m_element_size(element_size) {}
    ComponentStorage(const ComponentStorage &) = delete;
    ComponentStorage(ComponentStorage &&) = delete;
    constexpr ~ComponentStorage() { std::free(m_data); }

    ComponentStorage &operator=(const ComponentStorage &) = delete;
    ComponentStorage &operator=(ComponentStorage &&) = delete;

    void ensure_capacity(SizeType capacity) {
        if (capacity > m_capacity) {
            reallocate(capacity * 2);
        }
        m_use_list.resize(m_capacity + 1);
    }

    void obtain(SizeType index) {
        ASSERT(index < m_capacity);
        m_use_list[index] = true;
    }

    void release(SizeType index) {
        ASSERT(index < m_capacity);
        m_use_list[index] = false;
    }

    template <typename T>
    T *at(SizeType index) {
        if (index >= m_capacity) {
            return nullptr;
        }
        return m_use_list[index] ? reinterpret_cast<T *>(m_data) + index : nullptr;
    }

    SizeType capacity() const { return m_capacity; }
};
