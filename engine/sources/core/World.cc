#include <vull/core/World.hh>

#include <vull/core/System.hh>
#include <vull/support/Box.hh>
#include <vull/support/Vector.hh>

void World::update(float dt) {
    for (const auto &system : systems()) {
        system->update(this, dt);
    }
}
