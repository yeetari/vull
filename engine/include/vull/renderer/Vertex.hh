#pragma once

#include <glm/gtx/hash.hpp>
#include <glm/vec3.hpp>

#include <functional>

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
};

inline bool operator==(const Vertex &lhs, const Vertex &rhs) {
    return lhs.position == rhs.position && lhs.normal == rhs.normal;
}

namespace std {

template <>
struct hash<Vertex> {
    std::size_t operator()(const Vertex &vertex) const {
        return hash<glm::vec3>{}(vertex.position) ^ hash<glm::vec3>{}(vertex.normal);
    }
};

} // namespace std
