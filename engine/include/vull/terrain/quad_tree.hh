#pragma once

#include <vull/container/vector.hh>
#include <vull/maths/vec.hh>
#include <vull/terrain/chunk.hh>

namespace vull {

class QuadTree {
    Chunk m_root;

public:
    explicit QuadTree(float size) : m_root(Vec2f(0.0f), size) {}

    void subdivide(const Vec3f &point);
    void traverse(Vector<Chunk *> &chunks);
};

} // namespace vull
