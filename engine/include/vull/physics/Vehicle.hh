#pragma once

#include <vull/core/EntityId.hh>
#include <vull/core/System.hh>
#include <vull/support/Optional.hh>
#include <vull/support/Vector.hh>

#include <utility>

struct World;

class Wheel {
    const float m_radius;
    const float m_x_offset;
    Optional<EntityId> m_visual_entity;

    // Input.
    float m_engine_force{0.0f};
    float m_steering{0.0f};

    // Current state.
    float m_delta_rotation{0.0f};
    float m_rotation{0.0f};
    float m_roll{0.0f};
    float m_suspension_compression{0.0f};
    float m_suspension_length_prev{0.0f};
    float m_suspension_length{0.0f};

public:
    Wheel(float radius, float x_offset, Optional<EntityId> visual_entity = {})
        : m_radius(radius), m_x_offset(x_offset), m_visual_entity(visual_entity) {}

    void set_engine_force(float engine_force) { m_engine_force = engine_force; }
    void set_steering(float steering) { m_steering = steering; }

    void set_delta_rotation(float delta_rotation) { m_delta_rotation = delta_rotation; }
    void set_rotation(float rotation) { m_rotation = rotation; }
    void set_roll(float roll) { m_roll = roll; }
    void set_suspension_compression(float suspension_compression) { m_suspension_compression = suspension_compression; }
    void set_suspension_length_prev(float suspension_length_prev) { m_suspension_length_prev = suspension_length_prev; }
    void set_suspension_length(float suspension_length) { m_suspension_length = suspension_length; }

    float radius() const { return m_radius; }
    float x_offset() const { return m_x_offset; }
    const Optional<EntityId> &visual_entity() const { return m_visual_entity; }

    float engine_force() const { return m_engine_force; }
    float steering() const { return m_steering; }

    float delta_rotation() const { return m_delta_rotation; }
    float rotation() const { return m_rotation; }
    float roll() const { return m_roll; }
    float suspension_compression() const { return m_suspension_compression; }
    float suspension_length_prev() const { return m_suspension_length_prev; }
    float suspension_length() const { return m_suspension_length; }
};

class Axle {
    const float m_suspension_damping;
    const float m_suspension_stiffness;
    const float m_suspension_rest_length;
    const float m_z_offset;
    Vector<Wheel> m_wheels;

public:
    Axle(float suspension_damping, float suspension_stiffness, float suspension_rest_length, float z_offset)
        : m_suspension_damping(suspension_damping), m_suspension_stiffness(suspension_stiffness),
          m_suspension_rest_length(suspension_rest_length), m_z_offset(z_offset) {}

    template <typename... Args>
    Wheel &add_wheel(Args &&...args) {
        return m_wheels.emplace(std::forward<Args>(args)...);
    }

    float suspension_damping() const { return m_suspension_damping; }
    float suspension_stiffness() const { return m_suspension_stiffness; }
    float suspension_rest_length() const { return m_suspension_rest_length; }
    float z_offset() const { return m_z_offset; }
    Vector<Wheel> &wheels() { return m_wheels; }
    const Vector<Wheel> &wheels() const { return m_wheels; }
};

class Vehicle {
    Vector<Axle> m_axles;

public:
    template <typename... Args>
    Axle &add_axle(Args &&...args) {
        return m_axles.emplace(std::forward<Args>(args)...);
    }

    Vector<Axle> &axles() { return m_axles; }
    const Vector<Axle> &axles() const { return m_axles; }
};

struct VehicleSystem final : public System<VehicleSystem> {
    void update(World *world, float dt) override;
};
