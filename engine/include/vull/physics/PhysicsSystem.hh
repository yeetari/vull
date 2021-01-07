#pragma once

#include <vull/core/System.hh>

class Window;

class PhysicsSystem final : public System<PhysicsSystem> {
    const Window &m_window;

public:
    explicit PhysicsSystem(const Window &window) : m_window(window) {}

    void update(World *world, float dt) override;
};
