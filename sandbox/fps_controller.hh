#pragma once

#include <vull/ecs/entity.hh>
#include <vull/maths/common.hh>
#include <vull/maths/mat.hh>
#include <vull/maths/vec.hh>
#include <vull/scene/camera.hh>

namespace vull::platform {

class Window;

} // namespace vull::platform

class FpsController final : public vull::Camera {
    vull::Entity m_entity;
    vull::Vec3f m_forward;
    vull::Vec3f m_right;
    vull::Vec3f m_position;
    float m_pitch{0.0f};
    float m_yaw{0.0f};
    float m_aspect_ratio{1.0f};
    float m_fov{vull::half_pi<float>};

public:
    explicit FpsController(vull::Entity entity);

    void handle_mouse_move(vull::Vec2f delta);
    void update(const vull::platform::Window &window, float dt);

    float aspect_ratio() const override { return m_aspect_ratio; }
    float fov() const override { return m_fov; }

    vull::Vec3f position() const override { return m_position; }
    vull::Vec3f forward() const override { return m_forward; }
    vull::Vec3f right() const override { return m_right; }
    vull::Vec3f up() const override;

    vull::Mat4f projection_matrix() const override;
    vull::Mat4f view_matrix() const override;
};
