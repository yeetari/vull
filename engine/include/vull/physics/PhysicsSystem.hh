#pragma once

#include <vull/core/System.hh>

struct PhysicsSystem final : public System<PhysicsSystem> {
    void update(World *world, float dt) override;
};
