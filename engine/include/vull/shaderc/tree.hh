#pragma once

#include <vull/container/vector.hh>
#include <vull/support/utility.hh>

#include <stddef.h>
#include <stdint.h>

namespace vull::shaderc::tree {

class ArenaChunk {
    static constexpr size_t k_size = 65536;
    uint8_t *m_data;
    size_t m_head{0};

public:
    ArenaChunk() : m_data(new uint8_t[k_size]) {}
    ArenaChunk(const ArenaChunk &) = delete;
    ArenaChunk(ArenaChunk &&other)
        : m_data(vull::exchange(other.m_data, nullptr)), m_head(vull::exchange(other.m_head, 0u)) {}
    ~ArenaChunk() { delete[] m_data; }

    ArenaChunk &operator=(const ArenaChunk &) = delete;
    ArenaChunk &operator=(ArenaChunk &&) = delete;

    void *allocate(size_t size, size_t alignment);
};

class Arena {
    Vector<ArenaChunk> m_chunks;
    ArenaChunk *m_current_chunk;

public:
    Arena() : m_current_chunk(&m_chunks.emplace()) {}
    Arena(const Arena &) = delete;
    Arena(Arena &&other)
        : m_chunks(vull::move(other.m_chunks)), m_current_chunk(vull::exchange(other.m_current_chunk, nullptr)) {}
    ~Arena() = default;

    Arena &operator=(const Arena &) = delete;
    Arena &operator=(Arena &&) = delete;

    template <typename T, typename... Args>
    T *allocate(Args &&...args);
    template <typename T>
    void destroy(T *object);
};

template <typename BaseT, DerivedFrom<BaseT> T>
class NodeHandle {
    Arena &m_arena;
    T *m_node;

public:
    NodeHandle(Arena &arena, T *node) : m_arena(arena), m_node(node) {}
    NodeHandle(const NodeHandle &) = delete;
    NodeHandle(NodeHandle &&other) : m_arena(other.m_arena), m_node(vull::exchange(other.m_node, nullptr)) {}
    ~NodeHandle();

    NodeHandle &operator=(const NodeHandle &) = delete;
    NodeHandle &operator=(NodeHandle &&) = delete;

    operator NodeHandle<BaseT, BaseT>() const && { return NodeHandle<BaseT, BaseT>(m_arena, m_node); }

    T *disown() { return vull::exchange(m_node, nullptr); }
    T *operator->() const { return m_node; }
};

template <typename BaseT, DerivedFrom<BaseT> T>
NodeHandle(BaseT, T) -> NodeHandle<BaseT, T>;

template <typename T, typename... Args>
T *Arena::allocate(Args &&...args) {
    auto *bytes = m_current_chunk->allocate(sizeof(T), alignof(T));
    if (bytes == nullptr) {
        m_current_chunk = &m_chunks.emplace();
        bytes = m_current_chunk->allocate(sizeof(T), alignof(T));
    }
    return new (bytes) T(vull::forward<Args>(args)...);
}

template <typename T>
void Arena::destroy(T *object) {
    object->~T();
}

template <typename BaseT, DerivedFrom<BaseT> T>
NodeHandle<BaseT, T>::~NodeHandle() {
    if constexpr (!vull::is_same<BaseT, T>) {
        if (m_node != nullptr) {
            m_arena.destroy(m_node);
        }
    }
}

} // namespace vull::shaderc::tree
