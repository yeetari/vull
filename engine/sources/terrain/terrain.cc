#include <vull/terrain/terrain.hh>

#include <vull/container/vector.hh>
#include <vull/maths/common.hh>
#include <vull/maths/vec.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/terrain/noise.hh>
#include <vull/terrain/quad_tree.hh>

namespace vull {

class Chunk;

float Terrain::height(float x, float z) const {
    x /= m_size;
    z /= m_size;

    float amplitude = 1.0f;
    float frequency = 10.0f;
    float normalisation = 0.0f;
    float total = 0.0f;
    for (uint32_t octave = 0; octave < 5; octave++) {
        float noise = vull::perlin_noise(x * frequency, z * frequency, m_seed);
        normalisation += amplitude;
        total += noise * amplitude;
        amplitude *= vull::pow(2.0f, -0.71f);
        frequency *= 2.0f;
    }
    total /= normalisation;
    return vull::pow(total, 2.5f) * 200.0f;
}

void Terrain::update(const Vec3f &camera_position, Vector<Chunk *> &chunks) {
    // TODO: Only update when camera moves, would be beneficial with a frame time budget managing scheduler.
    m_quad_tree = vull::make_unique<QuadTree>(m_size);
    m_quad_tree->subdivide(camera_position);
    m_quad_tree->traverse(chunks);
}

} // namespace vull
