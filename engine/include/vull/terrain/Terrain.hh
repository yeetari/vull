#pragma once

#include <vull/container/Vector.hh>
#include <vull/maths/Vec.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/terrain/QuadTree.hh>

#include <stdint.h>

namespace vull {

class Chunk;

class Terrain {
    const uint32_t m_seed;
    uint32_t m_size;
    UniquePtr<QuadTree> m_quad_tree;

public:
    static float height(uint32_t seed, float x, float z);

    Terrain(uint32_t size, uint32_t seed) : m_seed(seed), m_size(size) {}

    void update(const Vec3f &camera_position, Vector<Chunk *> &chunks);
    void set_size(uint32_t size) { m_size = size; }

    uint32_t seed() const { return m_seed; }
    uint32_t size() const { return m_size; }
};

} // namespace vull
