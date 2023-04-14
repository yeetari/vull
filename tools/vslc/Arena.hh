#pragma once

#include <vull/container/Vector.hh>
#include <vull/support/Utility.hh>

#include <stddef.h>
#include <stdint.h>

class ArenaChunk {
    static constexpr size_t k_size = 65536;
    uint8_t *m_data;
    size_t m_head{0};

public:
    ArenaChunk() : m_data(new uint8_t[k_size]) {}
    ArenaChunk(const ArenaChunk &) = delete;
    ArenaChunk(ArenaChunk &&);
    ~ArenaChunk() { delete[] m_data; }

    ArenaChunk &operator=(const ArenaChunk &) = delete;
    ArenaChunk &operator=(ArenaChunk &&) = delete;

    void *allocate(size_t size, size_t alignment);
};

class Arena {
    vull::Vector<ArenaChunk> m_chunks;
    ArenaChunk *m_current_chunk;

public:
    Arena() : m_current_chunk(&m_chunks.emplace()) {}
    Arena(const Arena &) = delete;
    Arena(Arena &&);
    ~Arena() = default;

    Arena &operator=(const Arena &) = delete;
    Arena &operator=(Arena &&) = delete;

    template <typename U, typename... Args>
    U *allocate(Args &&...args);
    template <typename U>
    void destroy(U *ptr);
};

inline ArenaChunk::ArenaChunk(ArenaChunk &&other) {
    m_data = vull::exchange(other.m_data, nullptr);
    m_head = vull::exchange(other.m_head, 0u);
}

inline void *ArenaChunk::allocate(size_t size, size_t alignment) {
    size = (size + alignment - 1) & ~(alignment - 1);
    if (m_head + size >= k_size) {
        return nullptr;
    }
    auto *ptr = m_data + m_head;
    m_head += size;
    return ptr;
}

inline Arena::Arena(Arena &&other) {
    m_chunks = vull::move(other.m_chunks);
    m_current_chunk = vull::exchange(other.m_current_chunk, nullptr);
}

template <typename U, typename... Args>
U *Arena::allocate(Args &&...args) {
    auto *ptr = m_current_chunk->allocate(sizeof(U), alignof(U));
    if (ptr == nullptr) {
        m_current_chunk = &m_chunks.emplace();
        ptr = m_current_chunk->allocate(sizeof(U), alignof(U));
    }
    return new (ptr) U(vull::forward<Args>(args)...);
}

template <typename U>
void Arena::destroy(U *ptr) {
    ptr->~U();
}
