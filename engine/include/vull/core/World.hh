#pragma once

#include <vull/core/Entity.hh>
#include <vull/core/System.hh>

struct World : public EntityManager, public SystemManager {
    void update(float dt);
};
