#pragma once

#include <vull/ecs/Entity.hh>
#include <vull/support/Result.hh>
#include <vull/support/StreamError.hh>
#include <vull/support/StringView.hh>

namespace vull::vpak {

class Reader;
class Writer;

} // namespace vull::vpak

namespace vull {

enum class WorldError {
    InvalidComponent,
    MissingEntry,
};

class World : public EntityManager {
public:
    Result<void, StreamError, WorldError> deserialise(vpak::Reader &pack_reader, StringView name);
    Result<float, StreamError> serialise(vpak::Writer &pack_writer, StringView name);
};

} // namespace vull
