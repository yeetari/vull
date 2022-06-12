#pragma once

namespace vull {

class World;

class PhysicsEngine {
    void sub_step(World &world, float time_step);

public:
    void step(World &world, float dt);
};

} // namespace vull
