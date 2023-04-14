#pragma once

#include <vull/container/Vector.hh>
#include <vull/maths/Vec.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/terrain/QuadTree.hh>

#include <stdint.h>

namespace vull {

class Chunk;

class Terrain {
    const float m_size;
    const uint32_t m_seed;
    UniquePtr<QuadTree> m_quad_tree;

public:
    Terrain(float size, uint32_t seed) : m_size(size), m_seed(seed) {}

    float height(float x, float z) const;
    void update(const Vec3f &camera_position, Vector<Chunk *> &chunks);

    float size() const { return m_size; }
    uint32_t seed() const { return m_seed; }
};

} // namespace vull
