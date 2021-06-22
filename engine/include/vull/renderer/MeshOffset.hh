#pragma once

#include <glm/vec3.hpp>

class MeshOffset {
    const glm::vec3 m_offset;

public:
    explicit MeshOffset(const glm::vec3 &offset) : m_offset(offset) {}

    const glm::vec3 &offset() const { return m_offset; }
};
