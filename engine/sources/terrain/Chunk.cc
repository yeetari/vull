#include <vull/terrain/Chunk.hh>

#include <vull/maths/Common.hh>
#include <vull/maths/Vec.hh>
#include <vull/support/Array.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/support/Vector.hh>

namespace vull {

void Chunk::build_geometry(Vector<ChunkVertex> &vertices, Vector<uint32_t> &indices) {
    constexpr uint32_t tessellation_level = 32;
    uint32_t initial_size = vertices.size();

    float segment_size = (m_size * 2.0f) / tessellation_level;
    for (uint32_t z = 0; z < tessellation_level + 1; z++) {
        for (uint32_t x = 0; x < tessellation_level + 1; x++) {
            auto &vertex = vertices.emplace();
            vertex.position =
                Vec2f(static_cast<float>(x), static_cast<float>(z)) * segment_size + m_center - Vec2f(m_size);
        }
    }

    indices.ensure_capacity(indices.size() + tessellation_level * tessellation_level * 6);
    for (uint32_t z = 0; z < tessellation_level; z++) {
        for (uint32_t x = 0; x < tessellation_level; x++) {
            const auto a = initial_size + x + (tessellation_level + 1) * z;
            const auto b = initial_size + x + (tessellation_level + 1) * (z + 1);
            const auto c = initial_size + (x + 1) + (tessellation_level + 1) * (z + 1);
            const auto d = initial_size + (x + 1) + (tessellation_level + 1) * z;
            indices.push(a);
            indices.push(b);
            indices.push(d);
            indices.push(b);
            indices.push(c);
            indices.push(d);
        }
    }
}

void Chunk::subdivide(const Vec2f &point) {
    if (m_size <= 256.0f) {
        return;
    }

    float distance = vull::distance(m_center, point);
    if (distance > vull::hypot(m_size * 2.0f, m_size * 2.0f)) {
        return;
    }

    const float child_size = m_size * 0.5f;
    m_children[0] = vull::make_unique<Chunk>(m_center + Vec2f(-child_size, -child_size),
                                             child_size); // bottom left
    m_children[1] = vull::make_unique<Chunk>(m_center + Vec2f(child_size, -child_size),
                                             child_size); // bottom right
    m_children[2] = vull::make_unique<Chunk>(m_center + Vec2f(-child_size, child_size),
                                             child_size); // top left
    m_children[3] = vull::make_unique<Chunk>(m_center + Vec2f(child_size, child_size),
                                             child_size); // top right
    for (auto &child : m_children) {
        child->subdivide(point);
    }
}

void Chunk::traverse(Vector<Chunk *> &chunks) {
    if (is_leaf()) {
        chunks.push(this);
        return;
    }
    for (auto &child : m_children) {
        child->traverse(chunks);
    }
}

} // namespace vull
