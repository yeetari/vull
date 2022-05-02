#include "Camera.hh"

#include <vull/core/Window.hh>
#include <vull/maths/Common.hh>
#include <vull/maths/Mat.hh>
#include <vull/maths/Vec.hh>

namespace {

constexpr float k_mouse_sensitivity = 0.5f;
constexpr vull::Vec3f k_world_up(0.0f, 1.0f, 0.0f);

} // namespace

void Camera::update(const vull::Window &window, float dt) {
    // Handle any mouse movement.
    m_yaw += window.delta_x() * dt * k_mouse_sensitivity;
    m_pitch -= window.delta_y() * dt * k_mouse_sensitivity;
    m_pitch = vull::clamp(m_pitch, -vull::half_pi<float> + 0.001f, vull::half_pi<float> - 0.001f);

    // Update direction vectors.
    m_forward.set_x(vull::cos(m_yaw) * vull::cos(m_pitch));
    m_forward.set_y(vull::sin(m_pitch));
    m_forward.set_z(vull::sin(m_yaw) * vull::cos(m_pitch));
    m_forward = vull::normalise(m_forward);
    m_right = vull::normalise(vull::cross(m_forward, k_world_up));

    // Handle any keyboard input.
    const float speed = (window.is_key_down(vull::Key::Shift) ? 50.0f : 10.0f) * dt;
    if (window.is_key_down(vull::Key::W)) {
        m_position += m_forward * speed;
    }
    if (window.is_key_down(vull::Key::S)) {
        m_position -= m_forward * speed;
    }
    if (window.is_key_down(vull::Key::A)) {
        m_position -= m_right * speed;
    }
    if (window.is_key_down(vull::Key::D)) {
        m_position += m_right * speed;
    }
}

vull::Mat4f Camera::view_matrix() {
    return vull::look_at(m_position, m_position + m_forward, k_world_up);
}
