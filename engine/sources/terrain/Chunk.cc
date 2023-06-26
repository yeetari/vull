#include <vull/terrain/Chunk.hh>

#include <vull/container/Array.hh>
#include <vull/container/Vector.hh>
#include <vull/maths/Common.hh>
#include <vull/maths/Vec.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/support/Utility.hh>

namespace vull {

bool Chunk::in_sphere(const Vec3f &center, float radius) {
    float r2 = radius * radius;
    Vec3f c1 = Vec3f(m_position.x(), 0.0f, m_position.y());
    Vec3f c2 = Vec3f(m_position.x() + m_size, 200.0f, m_position.y() + m_size);
    float distX = 0.0f;
    float distY = 0.0f;
    float distZ = 0.0f;

    if (center.x() < c1.x())
        distX = (center.x() - c1.x());
    else if (center.x() > c2.x())
        distX = (center.x() - c2.x());
    //    if (center.y() < c1.y())
    //        distY = (center.y() - c1.y());
    //    else if (center.y() > c2.y())
    //        distY = (center.y() - c2.y());
    if (center.z() < c1.z())
        distZ = (center.z() - c1.z());
    else if (center.z() > c2.z())
        distZ = (center.z() - c2.z());

    Vec3f distV(distX, distY, distZ);
    return dot(distV, distV) <= r2;
}

bool Chunk::subdivide(const Vec3f &point, Vector<Chunk *> &chunks, uint32_t depth) {
    // Determines rendering LOD level distribution based on distance from the viewer.
    // Value of 2.0 should result in equal number of triangles displayed on screen
    // (in average) for all distances.
    // Values above 2.0 will result in more triangles on more distant areas, and vice versa.
    float detail_balance = 2.0f;

    Vector<float> ranges(6);
    float current_detail_balance = 1.0f;
    float total = 0.0f;
    for (uint32_t i = 0; i < ranges.size(); i++) {
        total += current_detail_balance;
        current_detail_balance *= detail_balance;
    }

    float sect = 20000.0f / total;
    float prev_pos = 0.0f;
    current_detail_balance = 1.0f;
    for (uint32_t i = 0; i < ranges.size(); i++) {
        ranges[ranges.size() - i - 1] = prev_pos + sect * current_detail_balance;
        prev_pos = ranges[ranges.size() - i - 1];
        current_detail_balance *= detail_balance;
    }

    prev_pos = 0.0f;
    Vector<float> morph_start;
    Vector<float> morph_end;
    for (uint32_t i = 0; i < ranges.size(); i++) {
        morph_end.push(ranges[ranges.size() - i - 1]);
        morph_start.push(prev_pos + (morph_end.last() - prev_pos) * 0.66f);
        prev_pos = morph_start.last();
    }

    m_morph_start = morph_start[depth];
    m_morph_end = morph_end[depth];
    m_morph_end = vull::lerp(m_morph_end, m_morph_start, 0.01f);

    float lod_range = ranges[depth];
    if (!in_sphere(point, lod_range)) {
        return false;
    }

    if (depth + 1 == ranges.size()) {
        chunks.push(this);
        return true;
    }

    if (!in_sphere(point, ranges[depth + 1])) {
        chunks.push(this);
        return true;
    }

    const auto child_size = m_size >> 1u;
    m_children[0] = vull::make_unique<Chunk>(m_position, child_size);                                 // TL
    m_children[1] = vull::make_unique<Chunk>(m_position + Vec2u(child_size, 0), child_size);          // TR
    m_children[2] = vull::make_unique<Chunk>(m_position + Vec2u(0, child_size), child_size);          // BL
    m_children[3] = vull::make_unique<Chunk>(m_position + Vec2u(child_size, child_size), child_size); // BR
    for (auto &child : m_children) {
        if (!child->subdivide(point, chunks, depth + 1)) {
            chunks.push(child.ptr());
        }
    }
    return true;
}

} // namespace vull
