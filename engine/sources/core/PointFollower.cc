#include <vull/core/PointFollower.hh>

#include <vull/core/Entity.hh>
#include <vull/core/Transform.hh>
#include <vull/core/World.hh>
#include <vull/support/Vector.hh>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

void PointFollowerSystem::update(World *world, float dt) {
    for (auto [entity, follower, transform] : world->view<PointFollower, Transform>()) {
        const auto &next_position = follower->m_points[follower->m_next_point];
        if (follower->m_mix_amount >= 1.0f) {
            follower->m_next_point = (follower->m_next_point + 1) % follower->m_points.size();
            follower->m_mix_amount = 0.0f;
            follower->m_start_position = transform->position();
            follower->m_start_orientation = transform->orientation();
        }

        auto next_orientation =
            glm::quatLookAt(glm::normalize(transform->position() - next_position), glm::vec3(0.0f, 1.0f, 0.0f));
        transform->position() = glm::mix(follower->m_start_position, next_position, follower->m_mix_amount);
        transform->orientation() = glm::slerp(follower->m_start_orientation, next_orientation, follower->m_mix_amount);

        // TODO: Scale mix amount delta based on the distance between the current point and the next point.
        follower->m_mix_amount += dt * follower->m_speed;
    }
}
