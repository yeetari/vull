#pragma once

#include <vull/maths/Mat.hh>
#include <vull/maths/Vec.hh>

namespace vull {

class Window;

} // namespace vull

class FreeCamera {
    vull::Vec3f m_position;
    vull::Vec3f m_forward;
    vull::Vec3f m_right;
    float m_pitch{0.0f};
    float m_yaw{0.0f};

public:
    void update(const vull::Window &window, float dt);
    vull::Mat4f view_matrix();

    void set_position(const vull::Vec3f &position) { m_position = position; }
    void set_pitch(float pitch) { m_pitch = pitch; }
    void set_yaw(float yaw) { m_yaw = yaw; }

    const vull::Vec3f &position() const { return m_position; }
    const vull::Vec3f &forward() const { return m_forward; }
    const vull::Vec3f &right() const { return m_right; }
    float pitch() const { return m_pitch; }
    float yaw() const { return m_yaw; }
};
