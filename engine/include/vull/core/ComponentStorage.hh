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
    explicit ComponentStorage(SizeType element_size) : m_element_size(element_size) {}
    ~ComponentStorage() { std::free(m_data); }

    void ensure_capacity(SizeType capacity) {
        if (capacity > m_capacity) {
            reallocate(capacity * 2);
        }
        for (SizeType i = m_use_list.size(); i < capacity + 1; i++) {
            m_use_list.push(false);
        }
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
        ASSERT(index < m_capacity);
        return m_use_list[index] ? reinterpret_cast<T *>(m_data) + index : nullptr;
    }
};
