#pragma once

#include <vull/support/Assert.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>

namespace vull {

// TODO: Paging.
template <typename I>
class SparseSet {
    Vector<I, I> m_dense;
    Vector<I, I> m_sparse;
    uint8_t *m_data{nullptr};

    void (*m_destruct)(void *){nullptr};
    void (*m_swap)(void *, void *){nullptr};
    size_t m_object_size{0};
    I m_capacity{0};

public:
    SparseSet() = default;
    SparseSet(const SparseSet &) = delete;
    SparseSet(SparseSet &&);
    ~SparseSet();

    SparseSet &operator=(const SparseSet &) = delete;
    SparseSet &operator=(SparseSet &&) = delete;

    template <typename T>
    void initialise();

    template <typename T>
    T &at(I index);
    bool contains(I index) const;
    template <typename T, typename... Args>
    void emplace(I index, Args &&...args);
    void remove(I index);

    auto dense_begin() { return m_dense.begin(); }
    auto dense_end() { return m_dense.end(); }
    template <typename T>
    T *storage_begin() {
        return reinterpret_cast<T *>(m_data);
    }
    template <typename T>
    T *storage_end() {
        return &reinterpret_cast<T *>(m_data)[m_dense.size()];
    }
};

template <typename I>
SparseSet<I>::SparseSet(SparseSet &&other) : m_dense(move(other.m_dense)), m_sparse(move(other.m_sparse)) {
    m_data = exchange(other.m_data, nullptr);
    m_destruct = exchange(other.m_destruct, nullptr);
    m_swap = exchange(other.m_swap, nullptr);
    m_object_size = exchange(other.m_object_size, 0u);
    m_capacity = exchange(other.m_capacity, 0u);
}

template <typename I>
SparseSet<I>::~SparseSet() {
    for (I i = m_dense.size(); i > 0; i--) {
        m_destruct(m_data + (i - 1) * m_object_size);
    }
    delete[] reinterpret_cast<uint8_t *>(m_data);
}

template <typename I>
template <typename T>
void SparseSet<I>::initialise() {
    m_destruct = +[](void *ptr) {
        static_cast<T *>(ptr)->~T();
    };
    m_swap = +[](void *lhs, void *rhs) {
        swap(*static_cast<T *>(lhs), *static_cast<T *>(rhs));
    };
    m_object_size = sizeof(T);
}

template <typename I>
template <typename T>
T &SparseSet<I>::at(I index) {
    VULL_ASSERT(contains(index));
    VULL_ASSERT_PEDANTIC(m_object_size == sizeof(T));
    return *reinterpret_cast<T *>(m_data + m_sparse[index] * m_object_size);
}

template <typename I>
bool SparseSet<I>::contains(I index) const {
    // TODO: Sentinel value optimisation.
    if (index >= m_sparse.size()) {
        return false;
    }
    if (m_sparse[index] >= m_dense.size()) {
        return false;
    }
    return m_dense[m_sparse[index]] == index;
}

template <typename I>
template <typename T, typename... Args>
void SparseSet<I>::emplace(I index, Args &&...args) {
    VULL_ASSERT(!contains(index));
    VULL_ASSERT_PEDANTIC(m_object_size == sizeof(T));
    m_sparse.ensure_size(index + 1);
    m_sparse[index] = m_dense.size();

    if (auto new_capacity = m_dense.size() + 1; new_capacity > m_capacity) {
        new_capacity = max(m_capacity * 2 + 1, new_capacity);
        auto *new_data = new uint8_t[new_capacity * sizeof(T)];
        if constexpr (!IsTriviallyCopyable<T>) {
            for (I i = 0; i < m_dense.size(); i++) {
                new (new_data + i * sizeof(T)) T(move(storage_begin<T>()[i]));
            }
            for (I i = m_dense.size(); i > 0; i--) {
                storage_begin<T>()[i - 1].~T();
            }
        } else if (!m_dense.empty()) {
            memcpy(new_data, m_data, m_dense.size() * sizeof(T));
        }
        delete[] m_data;
        m_data = new_data;
        m_capacity = new_capacity;
    }
    new (&reinterpret_cast<T *>(m_data)[m_dense.size()]) T(forward<Args>(args)...);
    m_dense.push(index);
}

// TODO: Alternate templated remove function that may be slightly faster when T is known.
template <typename I>
void SparseSet<I>::remove(I index) {
    VULL_ASSERT(contains(index));
    if (const I &dense_index = m_sparse[index]; m_dense[dense_index] != m_dense.last()) {
        m_sparse[m_dense.last()] = dense_index;
        swap(m_dense[dense_index], m_dense.last());
        m_swap(m_data + dense_index * m_object_size, m_data + (m_dense.size() - 1) * m_object_size);
    }
    m_dense.pop();
    m_destruct(m_data + m_dense.size() * m_object_size);
}

} // namespace vull
