#include <vull/terrain/quad_tree.hh>

#include <vull/container/vector.hh>
#include <vull/maths/vec.hh>
#include <vull/terrain/chunk.hh>

namespace vull {

void QuadTree::subdivide(const Vec3f &point) {
    m_root.subdivide({point.x(), point.z()});
}

void QuadTree::traverse(Vector<Chunk *> &chunks) {
    m_root.traverse(chunks);
}

} // namespace vull
