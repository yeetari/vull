#pragma once

#include <vull/core/Entity.hh>
#include <vull/core/EntityId.hh>
#include <vull/support/Span.hh> // IWYU pragma: keep

#include <glm/vec3.hpp>

#include <limits>
#include <utility>

struct World;

class Ray {
    World *m_world;
    glm::vec3 m_start_point;

    bool m_hit{false};
    float m_hit_distance{std::numeric_limits<float>::max()};
    EntityId m_hit_entity;
    glm::vec3 m_hit_point{0.0f};
    glm::vec3 m_hit_normal{0.0f};

public:
    Ray(World *world, const glm::vec3 &start_point, const glm::vec3 &direction, float max_distance,
        Span<EntityId> to_ignore);
    Ray(const Ray &) = delete;
    Ray(Ray &&other) noexcept
        : m_world(other.m_world), m_start_point(other.m_start_point), m_hit(std::exchange(other.m_hit, false)),
          m_hit_distance(other.m_hit_distance), m_hit_entity(other.m_hit_entity), m_hit_point(other.m_hit_point),
          m_hit_normal(other.m_hit_normal) {}
    ~Ray() = default;

    Ray &operator=(const Ray &) = delete;
    Ray &operator=(Ray &&other) noexcept {
        m_world = other.m_world;
        m_start_point = other.m_start_point;
        m_hit = std::exchange(other.m_hit, false);
        m_hit_distance = other.m_hit_distance;
        m_hit_entity = other.m_hit_entity;
        m_hit_point = other.m_hit_point;
        m_hit_normal = other.m_hit_normal;
        return *this;
    }

    auto operator<=>(const Ray &other) const { return m_hit_distance <=> other.m_hit_distance; }

    bool hit() const { return m_hit; }
    float hit_distance() const { return m_hit_distance; }
    Entity hit_entity() const;
    const glm::vec3 &hit_point() const { return m_hit_point; }
    const glm::vec3 &hit_normal() const { return m_hit_normal; }
    const glm::vec3 &start_point() const { return m_start_point; }
};
