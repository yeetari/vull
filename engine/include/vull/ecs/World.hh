#pragma once

#include <vull/ecs/Entity.hh>

namespace vull::vpak {

class Reader;
class Writer;

} // namespace vull::vpak

namespace vull {

class World : public EntityManager {
public:
    void deserialise(vpak::Reader &pack_reader);
    float serialise(vpak::Writer &pack_writer);
};

} // namespace vull
