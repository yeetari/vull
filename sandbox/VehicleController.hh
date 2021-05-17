#pragma once

#include <vull/core/System.hh>

class Window;
struct World;

struct VehicleController {};

class VehicleControllerSystem final : public System<VehicleControllerSystem> {
    const Window &m_window;
    float m_steering{0.0f};

public:
    explicit VehicleControllerSystem(const Window &window) : m_window(window) {}

    void update(World *world, float dt) override;
};
