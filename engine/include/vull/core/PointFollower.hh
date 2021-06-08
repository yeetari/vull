#pragma once

#include <vull/core/System.hh>
#include <vull/support/Vector.hh>

#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>

#include <cstdint>

struct PointFollowerSystem;
struct World;

class PointFollower {
    friend PointFollowerSystem;

private:
    const Vector<glm::vec3> &m_points;
    const float m_speed;
    float m_mix_amount{0.0f};
    std::uint32_t m_next_point{0};
    glm::vec3 m_start_position{0.0f};
    glm::quat m_start_orientation{};

public:
    PointFollower(const Vector<glm::vec3> &points, float speed) : m_points(points), m_speed(speed) {}
};

struct PointFollowerSystem final : public System<PointFollowerSystem> {
    void update(World *world, float dt) override;
};
