#pragma once

class PhysicsSystem;

class SphereCollider {
    friend PhysicsSystem;

private:
    const float m_radius;

public:
    explicit SphereCollider(float radius) : m_radius(radius) {}
};
