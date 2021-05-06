#pragma once

#include <vull/core/Entity.hh>
#include <vull/core/EntityId.hh>
#include <vull/support/Span.hh> // IWYU pragma: keep

#include <glm/vec3.hpp>

struct World;

class Ray {
    World *const m_world;
    bool m_hit{false};
    EntityId m_hit_entity;
    glm::vec3 m_hit_point{0.0f};
    glm::vec3 m_hit_normal{0.0f};

public:
    Ray(World *world, const glm::vec3 &ray_start, const glm::vec3 &ray_dir, float max_distance,
        Span<EntityId> to_ignore);

    bool hit() const { return m_hit; }
    Entity hit_entity() const;
    const glm::vec3 &hit_point() const { return m_hit_point; }
    const glm::vec3 &hit_normal() const { return m_hit_normal; }
};
