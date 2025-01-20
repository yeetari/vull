#pragma once

#include <vull/ecs/entity.hh>
#include <vull/support/result.hh>
#include <vull/support/string_view.hh>

namespace vull::vpak {

class Writer;

} // namespace vull::vpak

namespace vull {

enum class StreamError;
struct Stream;

enum class WorldError {
    InvalidComponent,
    MissingEntry,
};

class World : public EntityManager {
public:
    Result<void, StreamError, WorldError> deserialise(Stream &stream);
    Result<void, StreamError> serialise(vpak::Writer &pack_writer, StringView name);
};

} // namespace vull
