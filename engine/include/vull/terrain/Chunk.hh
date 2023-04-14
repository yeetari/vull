#pragma once

#include <vull/container/Array.hh>
#include <vull/container/Vector.hh>
#include <vull/maths/Vec.hh>
#include <vull/support/UniquePtr.hh>

#include <stdint.h>

namespace vull {

struct ChunkVertex {
    Vec2f position;
};

class Chunk {
    Array<UniquePtr<Chunk>, 4> m_children;
    const Vec2f m_center;
    const float m_size;

public:
    static void build_flat_mesh(Vector<ChunkVertex> &vertices, Vector<uint32_t> &indices, uint32_t tessellation_level);

    Chunk(const Vec2f &center, float size) : m_center(center), m_size(size) {}

    void subdivide(const Vec2f &point);
    void traverse(Vector<Chunk *> &chunks);

    bool is_leaf() const { return !m_children[0]; }
    const Vec2f &center() const { return m_center; }
    float size() const { return m_size; }
};

} // namespace vull
