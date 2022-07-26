#include <vull/ecs/World.hh>

#include <vull/core/Log.hh>
#include <vull/ecs/EntityId.hh>
#include <vull/ecs/SparseSet.hh>
#include <vull/support/Algorithm.hh>
#include <vull/support/Optional.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>
#include <vull/vpak/PackFile.hh>
#include <vull/vpak/Reader.hh>
#include <vull/vpak/Writer.hh>

#include <stdint.h>

namespace vull {

void World::deserialise(vpak::Reader &pack_reader) {
    auto stream = pack_reader.open("/world");
    if (!stream) {
        vull::error("[vpak] Missing /world entry");
        return;
    }

    // TODO(stream-api): templated read_varint.
    const auto entity_count = static_cast<EntityId>(stream->read_varint());
    m_entities.ensure_capacity(entity_count);
    for (EntityId i = 0; i < entity_count; i++) {
        m_entities.push(i);
    }

    const auto set_count = stream->read_varint();
    for (uint32_t i = 0; i < set_count; i++) {
        const auto set_entity_count = static_cast<EntityId>(stream->read_varint());
        if (set_entity_count == 0) {
            continue;
        }
        if (i >= m_component_sets.size() || !m_component_sets[i].initialised()) {
            vull::error("[vpak] Mismatched world");
            return;
        }
        auto &set = m_component_sets[i];
        set.deserialise(set_entity_count, [&] {
            return stream->read_byte();
        });
        for (EntityId j = 0; j < set_entity_count; j++) {
            set.raw_ensure_index(static_cast<EntityId>(stream->read_varint()));
        }
    }
}

float World::serialise(vpak::Writer &pack_writer) {
    auto entry = pack_writer.start_entry("/world", vpak::EntryType::WorldData);
    entry.write_varint(m_entities.size());
    entry.write_varint(m_component_sets.size());
    for (auto &set : m_component_sets) {
        entry.write_varint(set.size());
        if (!set.initialised()) {
            continue;
        }
        set.serialise([&](uint8_t byte) {
            entry.write_byte(byte);
        });
        for (EntityId id : vull::ViewAdapter(set.dense_begin(), set.dense_end())) {
            entry.write_varint(id);
        }
    }
    return entry.finish();
}

} // namespace vull
