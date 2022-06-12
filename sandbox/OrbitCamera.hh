#pragma once

#include <vull/maths/Mat.hh>
#include <vull/maths/Vec.hh>

namespace vull {

class Window;

} // namespace vull

class OrbitCamera {
    vull::Vec3f m_position;
    vull::Vec3f m_pivot;
    vull::Vec3f m_translated;
    float m_pitch{0.0f};
    float m_yaw{0.0f};

public:
    void update(const vull::Window &window, float dt);
    vull::Mat4f view_matrix();

    void set_position(const vull::Vec3f &position) { m_position = position; }
    void set_pivot(const vull::Vec3f &pivot) { m_pivot = pivot; }
    const vull::Vec3f &translated() const { return m_translated; }
};
