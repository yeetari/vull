#pragma once

#include <vull/core/System.hh>

class Window;
struct World;

struct PlayerController {};

class PlayerControllerSystem final : public System<PlayerControllerSystem> {
    const Window &m_window;
    bool m_space_pressed{false};

public:
    explicit PlayerControllerSystem(const Window &window) : m_window(window) {}

    void update(World *world, float dt) override;
};
