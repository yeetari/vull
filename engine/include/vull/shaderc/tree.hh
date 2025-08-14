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
};

class NodeBase {
    uint32_t m_ref_count{0};

public:
    void add_ref();
    bool sub_ref();
};

// TODO: Use SharedPtr?
template <typename BaseT, DerivedFrom<BaseT> T>
class NodeHandle {
    template <typename BaseU, DerivedFrom<BaseU>>
    friend class NodeHandle;

private:
    T *m_node{nullptr};

    explicit NodeHandle(T *node);

public:
    static NodeHandle create_new(T *node) { return NodeHandle(node); }

    NodeHandle() = default;
    NodeHandle(const NodeHandle &) = delete;
    NodeHandle(NodeHandle &&other) : m_node(vull::exchange(other.m_node, nullptr)) {}

    // Allow upcasting from derived node types.
    template <DerivedFrom<T> U>
    NodeHandle(NodeHandle<BaseT, U> &&other) : m_node(vull::exchange(other.m_node, nullptr)) {}

    ~NodeHandle();

    NodeHandle &operator=(const NodeHandle &) = delete;
    NodeHandle &operator=(NodeHandle &&);

    explicit operator bool() const { return m_node != nullptr; }
    bool is_null() const { return m_node == nullptr; }

    NodeHandle share() const { return NodeHandle(m_node); }

    T &operator*() const { return *m_node; }
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

template <typename BaseT, DerivedFrom<BaseT> T>
NodeHandle<BaseT, T>::NodeHandle(T *node) : m_node(node) {
    node->add_ref();
}

template <typename BaseT, DerivedFrom<BaseT> T>
NodeHandle<BaseT, T>::~NodeHandle() {
    if (m_node != nullptr && m_node->sub_ref()) {
        m_node->destroy();
    }
}

template <typename BaseT, DerivedFrom<BaseT> T>
NodeHandle<BaseT, T> &NodeHandle<BaseT, T>::operator=(NodeHandle &&other) {
    NodeHandle moved(vull::move(other));
    vull::swap(m_node, moved.m_node);
    return *this;
}

} // namespace vull::shaderc::tree
