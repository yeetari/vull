#pragma once

#include <vull/container/vector.hh>
#include <vull/maths/vec.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/terrain/quad_tree.hh>

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
