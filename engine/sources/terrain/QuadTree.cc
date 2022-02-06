#include <vull/terrain/QuadTree.hh>

#include <vull/maths/Vec.hh>
#include <vull/support/Vector.hh>
#include <vull/terrain/Chunk.hh>

namespace vull {

void QuadTree::subdivide(const Vec3f &point) {
    m_root.subdivide({point.x(), point.z()});
}

void QuadTree::traverse(Vector<Chunk *> &chunks) {
    m_root.traverse(chunks);
}

} // namespace vull
