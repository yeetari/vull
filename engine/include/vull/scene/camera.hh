#pragma once

#include <vull/maths/mat.hh>
#include <vull/maths/vec.hh>

namespace vull {

class Camera {
public:
    virtual float aspect_ratio() const = 0;
    virtual float fov() const = 0;

    virtual Vec3f position() const = 0;
    virtual Vec3f forward() const = 0;
    virtual Vec3f right() const = 0;
    virtual Vec3f up() const = 0;

    virtual Mat4f projection_matrix() const = 0;
    virtual Mat4f view_matrix() const = 0;
};

} // namespace vull
