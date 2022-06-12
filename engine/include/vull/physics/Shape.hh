#pragma once

#include <vull/maths/Mat.hh>
#include <vull/maths/Vec.hh>

namespace vull {

struct Shape {
    Shape() = default;
    Shape(const Shape &) = delete;
    Shape(Shape &&) = delete;
    virtual ~Shape() = default;

    Shape &operator=(const Shape &) = delete;
    Shape &operator=(Shape &&) = delete;

    virtual Vec3f furthest_point(const Vec3f &direction) const = 0;
    virtual Mat3f inertia_tensor(float mass) const = 0;
};

class BoxShape : public Shape {
    Vec3f m_half_extents;

public:
    BoxShape(const Vec3f &half_extents) : m_half_extents(half_extents) {}

    Vec3f furthest_point(const Vec3f &direction) const override;
    Mat3f inertia_tensor(float mass) const override;
};

} // namespace vull
