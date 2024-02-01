#pragma once

#include <vull/core/builtin_components.hh>
#include <vull/ecs/component.hh>
#include <vull/maths/vec.hh>

namespace vull {

class BoundingSphere {
    VULL_DECLARE_COMPONENT(BuiltinComponents::BoundingSphere);

private:
    Vec3f m_center;
    float m_radius;

public:
    BoundingSphere() = default;
    BoundingSphere(const Vec3f &center, float radius) : m_center(center), m_radius(radius) {}

    const Vec3f &center() const { return m_center; }
    float radius() const { return m_radius; }
};

} // namespace vull
