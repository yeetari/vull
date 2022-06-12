#include "OrbitCamera.hh"

#include <vull/core/Window.hh>
#include <vull/maths/Common.hh>
#include <vull/maths/Mat.hh>
#include <vull/maths/Vec.hh>

namespace {

constexpr vull::Vec3f k_world_up(0.0f, 1.0f, 0.0f);

} // namespace

void OrbitCamera::update(const vull::Window &window, float) {
    // Handle any mouse movement.
    m_yaw -= window.delta_x() * (2.0f * vull::pi<float> / static_cast<float>(window.width()));
    m_pitch -= window.delta_y() * (vull::pi<float> / static_cast<float>(window.height()));
    m_pitch = vull::clamp(m_pitch, -0.7f, 1.0f);

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
