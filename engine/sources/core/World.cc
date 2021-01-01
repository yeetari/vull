#include <vull/core/World.hh>

void World::update(float dt) {
    for (const auto &system : systems()) {
        system->update(this, dt);
    }
}
