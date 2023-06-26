#pragma once

#include <vull/container/Vector.hh>
#include <vull/maths/Vec.hh>
#include <vull/terrain/Chunk.hh>

namespace vull {

class QuadTree {
    Vector<Vector<UniquePtr<Chunk>>> m_root_chunks;

public:
    explicit QuadTree(uint32_t size);

    void subdivide(const Vec3f &point, Vector<Chunk *> &chunks);
};

} // namespace vull
