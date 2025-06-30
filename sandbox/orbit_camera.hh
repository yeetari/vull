#pragma once

#include <vull/maths/mat.hh>
#include <vull/maths/vec.hh>

namespace vull::platform {

class Window;

} // namespace vull::platform

class OrbitCamera {
    vull::Vec3f m_position;
    vull::Vec3f m_pivot;
    vull::Vec3f m_translated;
    float m_pitch{0.0f};
    float m_yaw{0.0f};

public:
    void handle_mouse_move(vull::Vec2f delta, const vull::platform::Window &window);
    void update();
    vull::Mat4f view_matrix();

    void set_position(const vull::Vec3f &position) { m_position = position; }
    void set_pivot(const vull::Vec3f &pivot) { m_pivot = pivot; }
    const vull::Vec3f &translated() const { return m_translated; }
};
