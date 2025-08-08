#include "fps_controller.hh"

#include <vull/core/input.hh>
#include <vull/ecs/entity.hh>
#include <vull/maths/common.hh>
#include <vull/maths/epsilon.hh>
#include <vull/maths/mat.hh>
#include <vull/maths/projection.hh>
#include <vull/maths/quat.hh>
#include <vull/maths/vec.hh>
#include <vull/physics/rigid_body.hh>
#include <vull/platform/window.hh>
#include <vull/scene/transform.hh>

using namespace vull;

namespace {

constexpr float k_mouse_sensitivity = 0.001f;
constexpr vull::Vec3f k_world_up(0.0f, 1.0f, 0.0f);

} // namespace

FpsController::FpsController(Entity entity) : m_entity(entity) {}

void FpsController::handle_mouse_move(Vec2f delta) {
    m_yaw += delta.x() * k_mouse_sensitivity;
    m_pitch -= delta.y() * k_mouse_sensitivity;
}

void FpsController::update(const platform::Window &window, float dt) {
    m_aspect_ratio = window.aspect_ratio();

    // Clamp pitch.
    m_pitch = vull::clamp(m_pitch, -vull::half_pi<float> + 0.001f, vull::half_pi<float> - 0.001f);

    // Update direction vectors.
    m_forward.set_x(vull::cos(m_yaw) * vull::cos(m_pitch));
    m_forward.set_y(vull::sin(m_pitch));
    m_forward.set_z(vull::sin(m_yaw) * vull::cos(m_pitch));
    m_forward = vull::normalise(m_forward);
    m_right = vull::normalise(vull::cross(m_forward, k_world_up));

    auto &transform = m_entity.get<Transform>();
    transform.set_rotation(vull::angle_axis(vull::half_pi<float> - m_yaw, k_world_up));

    Vec3f desired_direction(0.0f);
    if (window.is_key_pressed(Key::W)) {
        desired_direction += transform.forward();
    }
    if (window.is_key_pressed(Key::S)) {
        desired_direction -= transform.forward();
    }
    if (window.is_key_pressed(Key::A)) {
        desired_direction += transform.right();
    }
    if (window.is_key_pressed(Key::D)) {
        desired_direction -= transform.right();
    }

    auto &body = m_entity.get<RigidBody>();
    const float speed = window.is_key_pressed(Key::Shift) ? 15.0f : 10.0f;
    const float max_delta = dt * 50.0f;
    auto desired_velocity = desired_direction * speed;
    desired_velocity.set_y(body.linear_velocity().y());
    const auto step = desired_velocity - body.linear_velocity();
    const auto step_magnitude = vull::magnitude(step);

    if (vull::fuzzy_zero(step_magnitude) || step_magnitude <= max_delta) {
        body.set_linear_velocity(desired_velocity);
    } else {
        body.set_linear_velocity(body.linear_velocity() + (step / step_magnitude) * max_delta);
    }

    m_position = transform.position() + k_world_up;
}

Vec3f FpsController::up() const {
    return k_world_up;
}

Mat4f FpsController::projection_matrix() const {
    return vull::infinite_perspective(m_aspect_ratio, m_fov, 0.1f);
}

Mat4f FpsController::view_matrix() const {
    return vull::look_at(m_position, m_position + m_forward, k_world_up);
}
