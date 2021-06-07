#include <vull/physics/Vehicle.hh>

#include <vull/core/Entity.hh>
#include <vull/core/Transform.hh>
#include <vull/core/World.hh>
#include <vull/physics/Ray.hh>
#include <vull/physics/RigidBody.hh>
#include <vull/support/Array.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Optional.hh>
#include <vull/support/Vector.hh>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/epsilon.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/mat3x3.hpp>
#include <glm/matrix.hpp>
#include <glm/vec3.hpp>

#include <algorithm>

namespace {

float calculate_side_impulse(RigidBody *b1, RigidBody *b2, const Transform *t1, const Transform *t2,
                             const glm::vec3 &normal, const glm::vec3 &point) {
    if (glm::length2(normal) > 1.1f) {
        return 0.0f;
    }
    glm::vec3 r1 = point - t1->position();
    glm::vec3 r2 = point - t2->position();
    glm::vec3 v1 = b1->velocity_at_point(r1);
    glm::vec3 v2 = b2->velocity_at_point(r2);

    glm::vec3 inertia_a(b1->inertia_tensor()[0][0], b1->inertia_tensor()[1][1], b1->inertia_tensor()[2][2]);
    glm::vec3 inertia_b(b2->inertia_tensor()[0][0], b2->inertia_tensor()[1][1], b2->inertia_tensor()[2][2]);
    auto world_to_a = glm::transpose(glm::mat3(t1->matrix()));
    auto world_to_b = glm::transpose(glm::mat3(t2->matrix()));

    glm::vec3 aJ = world_to_a * glm::cross(r1, normal);
    glm::vec3 bJ = world_to_b * glm::cross(r2, -normal);
    glm::vec3 MinvJt1 = inertia_a * aJ;
    glm::vec3 MinvJt2 = inertia_b * bJ;
    float diag = b1->inv_mass() + glm::dot(MinvJt1, aJ) + b2->inv_mass() + glm::dot(MinvJt2, bJ);
    return -0.2f * glm::dot(normal, v1 - v2) * (1.0f / diag);
}

void update_wheel(Wheel &wheel, Axle &axle, const glm::vec3 &axle_position, World *world, Entity &entity,
                  RigidBody *chassis, const Transform *transform, float dt) {
    // Calculate wheel attachment position in world space.
    glm::vec3 wheel_position = axle_position + (transform->orientation() * glm::vec3(wheel.x_offset(), 0.0f, 0.0f));

    // Update visual transform, if available. We need to do this early before returning from a relaxed suspension.
    if (auto visual_entity = wheel.visual_entity();
        auto *visual_transform = world->get_component<Transform>(*visual_entity)) {
        visual_transform->position() =
            wheel_position - (transform->orientation() * glm::vec3(0.0f, wheel.suspension_length(), 0.0f));

        auto &orientation = visual_transform->orientation();
        orientation = transform->orientation();
        orientation = glm::rotate(orientation, wheel.steering(), glm::vec3(0.0f, 1.0f, 0.0f));
        orientation = glm::rotate(orientation, wheel.rotation(), glm::vec3(1.0f, 0.0f, 0.0f));
        orientation = glm::rotate(orientation, wheel.roll(), glm::vec3(0.0f, 0.0f, 1.0f));
    }

    // Perform multiple raycasts down from the wheel, ignoring the chassis entity. We perform multiple raycasts to
    // reduce wheel clipping and "jumping" when the wheel suddenly collides with a higher or lower surface.
    Array to_ignore{entity.id()};
    auto fire_ray = [&](float z_offset) -> Ray {
        glm::vec3 ray_position = wheel_position + (transform->orientation() * glm::vec3(0.0f, 0.0f, z_offset));
        return {world, ray_position, -transform->up(), axle.suspension_rest_length() + wheel.radius(), to_ignore};
    };
    Array rays{fire_ray(-0.8f), fire_ray(0.0f), fire_ray(0.8f)};
    float range = glm::abs(std::max_element(rays.begin(), rays.end())->hit_distance() -
                           std::min_element(rays.begin(), rays.end())->hit_distance());
    auto &ray = range < 0.1f ? rays[1] : *std::min_element(rays.begin(), rays.end());

    // Calculate suspension state.
    float suspension_distance = glm::abs(ray.start_point().y - ray.hit_point().y);
    if (suspension_distance > (axle.suspension_rest_length() + wheel.radius()) || !ray.hit()) {
        // Wheel is in air - suspension is relaxed.
        wheel.set_suspension_length_prev(axle.suspension_rest_length());
        wheel.set_suspension_length(axle.suspension_rest_length());
        wheel.set_suspension_compression(0.0f);
        return;
    }

    // Else, suspension is compressed.
    wheel.set_suspension_length_prev(wheel.suspension_length());
    wheel.set_suspension_length(suspension_distance - wheel.radius());
    wheel.set_suspension_compression(axle.suspension_rest_length() - wheel.suspension_length());

    // Calculate relative impulse point from ray hit point.
    // TODO: Configurable center of mass.
    constexpr glm::vec3 com(0.0f, -2.5f, 0.25f);
    glm::vec3 impulse_point = ray.hit_point() - transform->local_to_world(com);

    float denominator = glm::dot(ray.hit_normal(), -transform->up());
    float proj_vel = glm::dot(ray.hit_normal(), chassis->velocity_at_point(impulse_point));
    float suspension_relative_velocity = 0.0f;
    float clipped_inv_contact_dot_suspension = 10.0f;
    if (denominator < -0.1f) {
        float inv = -1.0f / denominator;
        suspension_relative_velocity = proj_vel * inv;
        clipped_inv_contact_dot_suspension = inv;
    }

    // Apply suspension force.
    float spring_force =
        wheel.suspension_compression() * axle.suspension_stiffness() * clipped_inv_contact_dot_suspension;
    float damper_force = suspension_relative_velocity * axle.suspension_damping();
    float suspension_force = (spring_force - damper_force) * chassis->mass();
    suspension_force = glm::max(suspension_force, 0.0f);
    suspension_force *= glm::dot(ray.hit_normal(), transform->up());
    chassis->apply_force(transform->up() * suspension_force, impulse_point);

    // Ground rigid body should never be nullptr since all physics objects are required to have a RigidBody.
    auto *ground = ray.hit_entity().get<RigidBody>();
    ASSERT(ground != nullptr);

    glm::quat wheel_orientation = glm::rotate(transform->orientation(), wheel.steering(), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::vec3 wheel_left = wheel_orientation * glm::vec3(-1.0f, 0.0f, 0.0f);
    float proj = glm::dot(wheel_left, ray.hit_normal());
    wheel_left -= ray.hit_normal() * proj;
    wheel_left = glm::normalize(wheel_left);
    float side_impulse = calculate_side_impulse(chassis, ground, transform, ray.hit_entity().get<Transform>(),
                                                wheel_left, ray.hit_point());

    float forward_impulse = wheel.engine_force() / wheel.radius();
    if (glm::epsilonEqual(wheel.engine_force(), 0.0f, glm::epsilon<float>())) {
        // TODO: Else calculate rolling friction.
    }

    float max_impulse = suspension_force * 5.0f;
    float max_impulse_sqrd = max_impulse * max_impulse;
    float x = forward_impulse * 0.5f;
    float y = side_impulse * 1.0f;
    float impulse_sqrd = (x * x + y * y);
    if (impulse_sqrd > max_impulse_sqrd) {
        float skid_factor = max_impulse / glm::sqrt(impulse_sqrd);
        forward_impulse *= skid_factor;
        side_impulse *= skid_factor;
    }

    chassis->apply_force(transform->forward() * forward_impulse, impulse_point);

    glm::vec3 friction_impulse = wheel_left * side_impulse;
    glm::vec3 impulse_point_chassis =
        impulse_point - (transform->up() * (glm::dot(transform->up(), impulse_point) * (1.0f - 0.1f)));
    glm::vec3 impulse_point_ground = ray.hit_point() - ray.hit_entity().get<Transform>()->position();
    chassis->apply_impulse(friction_impulse, impulse_point_chassis);
    ground->apply_impulse(-friction_impulse, impulse_point_ground);

    // Calculate delta rotation.
    float proj1 = glm::dot(transform->forward(), transform->up());
    float proj2 = glm::dot(transform->forward() - ray.hit_point() * proj1, chassis->linear_velocity());
    wheel.set_delta_rotation((proj2 * dt) / wheel.radius());
}

void update_axle(Axle &axle, World *world, Entity &entity, RigidBody *chassis, const Transform *transform, float dt) {
    // Calculate axle position in world space.
    glm::vec3 axle_position = transform->local_to_world(glm::vec3(0.0f, 0.0f, axle.z_offset()));
    for (auto &wheel : axle.wheels()) {
        update_wheel(wheel, axle, axle_position, world, entity, chassis, transform, dt);
    }
    for (auto &wheel : axle.wheels()) {
        // Update rotation and apply some rotation damping.
        wheel.set_rotation(wheel.rotation() + wheel.delta_rotation());
        wheel.set_delta_rotation(wheel.delta_rotation() * 0.99f);
    }
}

} // namespace

void VehicleSystem::update(World *world, float dt) {
    for (auto [entity, chassis, transform, vehicle] : world->view<RigidBody, Transform, Vehicle>()) {
        for (auto &axle : vehicle->axles()) {
            update_axle(axle, world, entity, chassis, transform, dt);
        }
    }
}
