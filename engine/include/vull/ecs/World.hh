#pragma once

#include <vull/ecs/Entity.hh>

namespace vull {

class PackReader;
class PackWriter;

class World : public EntityManager {
public:
    void deserialise(PackReader &pack_reader);
    float serialise(PackWriter &pack_writer);
};

} // namespace vull
