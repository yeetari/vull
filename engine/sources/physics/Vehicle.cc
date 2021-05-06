#include <vull/physics/Vehicle.hh>

#include <vull/core/Entity.hh>
#include <vull/core/Transform.hh>
#include <vull/core/World.hh>
#include <vull/physics/Ray.hh>
#include <vull/physics/RigidBody.hh>
#include <vull/support/Array.hh>
#include <vull/support/Optional.hh>
#include <vull/support/Vector.hh>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>

namespace {

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

    // Perform a raycast down from the wheel, ignoring the chassis entity.
    Array to_ignore{entity.id()};
    Ray ray(world, wheel_position, -transform->up(), axle.suspension_rest_length() + wheel.radius(), to_ignore);
    float ground_height = ray.hit() ? ray.hit_point().y : wheel_position.y;

    // Calculate suspension state.
    float suspension_distance = glm::abs(wheel_position.y - ground_height);
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

    // Apply suspension force.
    float spring_force = wheel.suspension_compression() * axle.suspension_stiffness();
    float damper_force = (wheel.suspension_length_prev() - wheel.suspension_length()) * axle.suspension_damping();
    float suspension_force = (spring_force + damper_force) * chassis->mass();
    suspension_force *= glm::dot(ray.hit_normal(), transform->up());
    chassis->apply_impulse(transform->up() * suspension_force, impulse_point);

    // Apply forward (engine) force.
    float forward_force = wheel.engine_force() / wheel.radius();
    chassis->apply_force(transform->forward() * forward_force, impulse_point);

    // Calculate lateral (slide) force.
    // TODO: This would be much nicer and simpler if the wheel always had its own transform/orientation.
    glm::quat wheel_orientation = glm::rotate(transform->orientation(), wheel.steering(), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::vec3 wheel_velocity = chassis->linear_velocity();
    glm::vec3 wheel_right = wheel_orientation * glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 wheel_fwd = wheel_orientation * glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 lateral_velocity = glm::dot(wheel_velocity, wheel_right) * wheel_right;
    glm::vec3 forward_velocity = glm::dot(wheel_velocity, wheel_fwd) * wheel_fwd;
    glm::vec3 slip = (forward_velocity + lateral_velocity) * 0.5f;

    float lateral_friction =
        glm::length(glm::dot(wheel_right, slip) * slip) * glm::length(suspension_force) / 9.81f / dt * 0.8f;
    glm::vec3 friction_force =
        -glm::normalize(glm::dot(slip, lateral_velocity) * lateral_velocity + glm::vec3(glm::epsilon<float>())) *
        lateral_friction;
    chassis->apply_force(friction_force, impulse_point);

    // TODO: This isn't right.
    if (auto *body = ray.hit_entity().get<RigidBody>()) {
        body->apply_force(-friction_force, ray.hit_point() - ray.hit_entity().get<Transform>()->position());
    }

    // Calculate delta rotation.
    float proj1 = glm::dot(transform->forward(), transform->up());
    float proj2 = glm::dot(transform->forward() - ray.hit_point() * proj1, wheel_velocity);
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
