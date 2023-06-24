#include <vull/ecs/World.hh>

#include <vull/container/Vector.hh>
#include <vull/core/Log.hh>
#include <vull/ecs/EntityId.hh>
#include <vull/ecs/SparseSet.hh>
#include <vull/support/Algorithm.hh>
#include <vull/support/Result.hh>
#include <vull/support/Stream.hh>
#include <vull/support/StringView.hh>
#include <vull/vpak/PackFile.hh>
#include <vull/vpak/Writer.hh>

#include <stdint.h>

namespace vull {

enum class StreamError;

Result<void, StreamError, WorldError> World::deserialise(Stream &stream) {
    const auto entity_count = VULL_TRY(stream.read_varint<EntityId>());
    m_entities.ensure_capacity(entity_count);
    for (EntityId i = 0; i < entity_count; i++) {
        m_entities.push(i);
    }

    const auto set_count = VULL_TRY(stream.read_varint<uint32_t>());
    for (uint32_t i = 0; i < set_count; i++) {
        const auto set_entity_count = VULL_TRY(stream.read_varint<EntityId>());
        if (set_entity_count == 0) {
            continue;
        }
        if (i >= m_component_sets.size() || !m_component_sets[i].initialised()) {
            vull::error("[vpak] Mismatched world");
            return WorldError::InvalidComponent;
        }
        auto &set = m_component_sets[i];
        set.deserialise(set_entity_count, stream);
        for (EntityId j = 0; j < set_entity_count; j++) {
            set.raw_ensure_index(VULL_TRY(stream.read_varint<EntityId>()));
        }
    }
    return {};
}

Result<float, StreamError> World::serialise(vpak::Writer &pack_writer, StringView name) {
    auto entry = pack_writer.start_entry(name, vpak::EntryType::World);
    VULL_TRY(entry.write_varint(m_entities.size()));
    VULL_TRY(entry.write_varint(m_component_sets.size()));
    for (auto &set : m_component_sets) {
        VULL_TRY(entry.write_varint(set.size()));
        if (!set.initialised()) {
            continue;
        }
        set.serialise(entry);
        for (EntityId id : vull::ViewAdapter(set.dense_begin(), set.dense_end())) {
            VULL_TRY(entry.write_varint(id));
        }
    }
    return entry.finish();
}

} // namespace vull
