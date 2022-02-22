#pragma once

#include <vull/maths/Vec.hh>
#include <vull/support/Array.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/support/Vector.hh>

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
    Chunk(const Vec2f &center, float size) : m_center(center), m_size(size) {}

    void build_geometry(Vector<ChunkVertex> &vertices, Vector<uint32_t> &indices);
    void subdivide(const Vec2f &point);
    void traverse(Vector<Chunk *> &chunks);

    bool is_leaf() const { return !m_children[0]; }
};

} // namespace vull