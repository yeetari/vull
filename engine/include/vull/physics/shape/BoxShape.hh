#pragma once

#include <vull/physics/shape/Shape.hh>

#include <glm/vec3.hpp>

class BoxShape final : public Shape {
    const glm::vec3 m_half_size;

public:
    explicit BoxShape(const glm::vec3 &m_half_size) : m_half_size(m_half_size) {}

    glm::vec3 support_point(const glm::vec3 &dir) const override {
        glm::vec3 ret;
        ret.x = glm::sign(dir.x) * m_half_size.x;
        ret.y = glm::sign(dir.y) * m_half_size.y;
        ret.z = glm::sign(dir.z) * m_half_size.z;
        return ret;
    }

    const glm::vec3 &half_size() const { return m_half_size; }
};
