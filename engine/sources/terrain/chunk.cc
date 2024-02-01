#include <vull/terrain/chunk.hh>

#include <vull/container/array.hh>
#include <vull/container/vector.hh>
#include <vull/maths/common.hh>
#include <vull/maths/vec.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>

namespace vull {

void Chunk::build_flat_mesh(Vector<ChunkVertex> &vertices, Vector<uint32_t> &indices, uint32_t tessellation_level) {
    for (uint32_t z = 0; z <= tessellation_level; z++) {
        for (uint32_t x = 0; x <= tessellation_level; x++) {
            auto &vertex = vertices.emplace();
            vertex.position =
                Vec2f(static_cast<float>(x), static_cast<float>(z)) / static_cast<float>(tessellation_level);
        }
    }

    indices.ensure_capacity(tessellation_level * tessellation_level * 6);
    for (uint32_t z = 0; z < tessellation_level; z++) {
        for (uint32_t x = 0; x < tessellation_level; x++) {
            const auto a = x + (tessellation_level + 1) * z;
            const auto b = x + (tessellation_level + 1) * (z + 1);
            const auto c = (x + 1) + (tessellation_level + 1) * (z + 1);
            const auto d = (x + 1) + (tessellation_level + 1) * z;
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
