#pragma once

#include <vull/container/Vector.hh>
#include <vull/maths/Vec.hh>
#include <vull/terrain/Chunk.hh>

namespace vull {

class QuadTree {
    Chunk m_root;

public:
    explicit QuadTree(float size) : m_root(Vec2f(0.0f), size) {}

    void subdivide(const Vec3f &point);
    void traverse(Vector<Chunk *> &chunks);
};

} // namespace vull
