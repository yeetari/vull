#include "orbit_camera.hh"

#include <vull/maths/common.hh>
#include <vull/maths/mat.hh>
#include <vull/maths/vec.hh>
#include <vull/platform/window.hh>

namespace {

constexpr vull::Vec3f k_world_up(0.0f, 1.0f, 0.0f);

} // namespace

void OrbitCamera::handle_mouse_move(vull::Vec2f delta, const vull::Window &window) {
    // Handle any mouse movement.
    delta *= 0.3f;
    m_yaw -= delta.x() * (2.0f * vull::pi<float> / static_cast<float>(window.resolution().x()));
    m_pitch -= delta.y() * (vull::pi<float> / static_cast<float>(window.resolution().y()));
    m_pitch = vull::clamp(m_pitch, -0.7f, 1.0f);
}

void OrbitCamera::update() {
    const auto forward = vull::normalise(m_pivot - m_translated);
    const auto right = vull::normalise(vull::cross(forward, k_world_up) + 1e-9f);
    vull::Vec4f focus_vector(m_position - m_pivot, 1.0f);

    focus_vector = vull::rotation_y(m_yaw) * focus_vector;
    focus_vector = vull::rotation(m_pitch, right) * focus_vector;
    m_translated = vull::Vec3f(focus_vector) + m_pivot;
}

vull::Mat4f OrbitCamera::view_matrix() {
    return vull::look_at(m_translated, m_pivot, k_world_up);
}
