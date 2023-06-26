#include <vull/terrain/QuadTree.hh>

#include <vull/container/Vector.hh>
#include <vull/maths/Vec.hh>
#include <vull/terrain/Chunk.hh>

namespace vull {

QuadTree::QuadTree(uint32_t size) {
    const uint32_t root_node_size = 128u;
    const uint32_t grid_size = vull::ceil_div(size, root_node_size);
    m_root_chunks.ensure_size(grid_size);
    for (uint32_t i = 0; i < grid_size; i++) {
        m_root_chunks[i].ensure_size(grid_size);
        for (uint32_t j = 0; j < grid_size; j++) {
            Vec2u position(i * root_node_size, j * root_node_size);
            m_root_chunks[i][j] = vull::make_unique<Chunk>(position, root_node_size);
        }
    }
}

void QuadTree::subdivide(const Vec3f &point, Vector<Chunk *> &chunks) {
    for (const auto &column : m_root_chunks) {
        for (const auto &chunk : column) {
            chunk->subdivide(point, chunks, 0);
        }
    }
}

} // namespace vull
