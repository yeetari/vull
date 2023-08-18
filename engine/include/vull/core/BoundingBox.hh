#pragma once

#include <vull/core/BuiltinComponents.hh>
#include <vull/ecs/Component.hh>
#include <vull/maths/Common.hh>
#include <vull/maths/Relational.hh>
#include <vull/maths/Vec.hh>
#include <vull/scene/Transform.hh>

namespace vull {

class BoundingBox {
    VULL_DECLARE_COMPONENT(BuiltinComponents::BoundingBox);

private:
    Vec3f m_center;
    Vec3f m_half_extents;

public:
    BoundingBox() = default;
    BoundingBox(const Vec3f &center, const Vec3f &half_extents) : m_center(center), m_half_extents(half_extents) {}

    bool contains(const Vec3f &point) const;
    BoundingBox transformed(const Transform &transform) const;

    const Vec3f &center() const { return m_center; }
    const Vec3f &half_extents() const { return m_half_extents; }
};

inline bool BoundingBox::contains(const Vec3f &point) const {
    return vull::all(vull::less_than_equal(vull::abs(m_center - point), m_half_extents));
}

inline BoundingBox BoundingBox::transformed(const Transform &transform) const {
    // TODO: Avoid mat3 cast?
    Vec3f center;
    Vec3f half_extents;
    const auto rot_mat = vull::to_mat3(transform.rotation());
    for (unsigned i = 0; i < 3; i++) {
        for (unsigned j = 0; j < 3; j++) {
            center[i] += rot_mat[i][j] * m_center[j];
            half_extents[i] += vull::abs(rot_mat[i][j]) * m_half_extents[j];
        }
    }
    return {transform.position() + center, half_extents * transform.scale()};
}

} // namespace vull
