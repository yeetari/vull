#pragma once

#include <vull/maths/common.hh>
#include <vull/maths/mat.hh>
#include <vull/maths/vec.hh>
#include <vull/scene/camera.hh>

namespace vull::platform {

class Window;

} // namespace vull::platform

class FreeCamera final : public vull::Camera {
    float m_aspect_ratio{1.0f};
    vull::Vec3f m_position;
    vull::Vec3f m_forward;
    vull::Vec3f m_right;
    float m_pitch{0.0f};
    float m_yaw{0.0f};
    float m_fov{vull::half_pi<float>};

public:
    void handle_mouse_move(vull::Vec2f delta);
    void update(const vull::platform::Window &window, float dt);

    void set_position(const vull::Vec3f &position) { m_position = position; }
    void set_pitch(float pitch) { m_pitch = pitch; }
    void set_yaw(float yaw) { m_yaw = yaw; }
    void set_fov(float fov) { m_fov = fov; }

    float aspect_ratio() const override { return m_aspect_ratio; }
    float fov() const override { return m_fov; }
    float pitch() const { return m_pitch; }
    float yaw() const { return m_yaw; }

    vull::Vec3f position() const override { return m_position; }
    vull::Vec3f forward() const override { return m_forward; }
    vull::Vec3f right() const override { return m_right; }
    vull::Vec3f up() const override;

    vull::Mat4f projection_matrix() const override;
    vull::Mat4f view_matrix() const override;
};
