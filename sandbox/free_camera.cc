#include "free_camera.hh"

#include <vull/core/input.hh>
#include <vull/core/window.hh>
#include <vull/maths/common.hh>
#include <vull/maths/mat.hh>
#include <vull/maths/projection.hh>
#include <vull/maths/vec.hh>

namespace {

constexpr float k_mouse_sensitivity = 0.0008f;
constexpr vull::Vec3f k_world_up(0.0f, 1.0f, 0.0f);

} // namespace

void FreeCamera::handle_mouse_move(vull::Vec2f delta) {
    // Handle any mouse movement.
    m_yaw += delta.x() * k_mouse_sensitivity;
    m_pitch -= delta.y() * k_mouse_sensitivity;
    m_pitch = vull::clamp(m_pitch, -vull::half_pi<float> + 0.001f, vull::half_pi<float> - 0.001f);

    // Update direction vectors.
    m_forward.set_x(vull::cos(m_yaw) * vull::cos(m_pitch));
    m_forward.set_y(vull::sin(m_pitch));
    m_forward.set_z(vull::sin(m_yaw) * vull::cos(m_pitch));
    m_forward = vull::normalise(m_forward);
    m_right = vull::normalise(vull::cross(m_forward, k_world_up));
}

void FreeCamera::update(const vull::Window &window, float dt) {
    // Handle any keyboard input.
    const float speed = (window.is_key_pressed(vull::Key::Shift) ? 50.0f : 10.0f) * dt;
    if (window.is_key_pressed(vull::Key::W)) {
        m_position += m_forward * speed;
    }
    if (window.is_key_pressed(vull::Key::S)) {
        m_position -= m_forward * speed;
    }
    if (window.is_key_pressed(vull::Key::A)) {
        m_position -= m_right * speed;
    }
    if (window.is_key_pressed(vull::Key::D)) {
        m_position += m_right * speed;
    }
}

vull::Vec3f FreeCamera::up() const {
    return k_world_up;
}

vull::Mat4f FreeCamera::projection_matrix() const {
    return vull::infinite_perspective(m_aspect_ratio, m_fov, 0.1f);
}

vull::Mat4f FreeCamera::view_matrix() const {
    return vull::look_at(m_position, m_position + m_forward, k_world_up);
}
