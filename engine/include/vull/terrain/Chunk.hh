#pragma once

#include <vull/container/Array.hh>
#include <vull/container/Vector.hh>
#include <vull/maths/Vec.hh>
#include <vull/support/UniquePtr.hh>

#include <stdint.h>

namespace vull {

class Chunk {
    const Vec2u m_position;
    const uint32_t m_size;
    Array<UniquePtr<Chunk>, 4> m_children;
    float m_morph_start{0.0f};
    float m_morph_end{0.0f};

public:
    Chunk(Vec2u position, uint32_t size) : m_position(position), m_size(size) {}

    bool in_sphere(const Vec3f &center, float radius);
    bool subdivide(const Vec3f &point, Vector<Chunk *> &chunks, uint32_t depth);

    bool is_leaf() const { return !m_children[0]; }
    Vec2u position() const { return m_position; }
    uint32_t size() const { return m_size; }
    float morph_start() const { return m_morph_start; }
    float morph_end() const { return m_morph_end; }
};

} // namespace vull
