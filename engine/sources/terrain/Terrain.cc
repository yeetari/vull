#include <vull/terrain/Terrain.hh>

#include <vull/container/Vector.hh>
#include <vull/core/Log.hh>
#include <vull/maths/Common.hh>
#include <vull/maths/Vec.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/terrain/Noise.hh>
#include <vull/terrain/QuadTree.hh>
#include <vull/terrain/SimplexNoise.hh>

namespace vull {

class Chunk;

float Terrain::height(uint32_t, float x, float z) {
    float nx = x / 2048.0f - 0.5f;
    float nz = z / 2048.0f - 0.5f;
    float total = 0.0f;
    SimplexNoise noise;
    total += noise.noise(nx, nz);
    total += noise.noise(nx * 2.0f, nz * 2.0f) * 0.5f;
    total += noise.noise(nx * 4.0f, nz * 4.0f) * 0.25f;
    return total / (1.0f + 0.5f + 0.25f) * 50.0f;
}

void Terrain::update(const Vec3f &camera_position, Vector<Chunk *> &chunks) {
    m_quad_tree = vull::make_unique<QuadTree>(m_size);
    m_quad_tree->subdivide(camera_position, chunks);
}

} // namespace vull
