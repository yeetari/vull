#pragma once

#include <vull/core/BuiltinComponents.hh>
#include <vull/ecs/Component.hh>
#include <vull/ecs/EntityId.hh>
#include <vull/maths/Mat.hh>

namespace vull {

class Transform {
    VULL_DECLARE_COMPONENT(BuiltinComponents::Transform);

private:
    EntityId m_parent;
    Mat4f m_matrix;

public:
    Transform(EntityId parent, const Mat4f &matrix) : m_parent(parent), m_matrix(matrix) {}

    EntityId parent() const { return m_parent; }
    const Mat4f &matrix() const { return m_matrix; }
};

} // namespace vull
