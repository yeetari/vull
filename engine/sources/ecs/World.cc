#include <vull/ecs/World.hh>

#include <vull/core/PackFile.hh>
#include <vull/core/PackReader.hh>
#include <vull/core/PackWriter.hh>
#include <vull/ecs/Entity.hh>
#include <vull/ecs/EntityId.hh>
#include <vull/ecs/SparseSet.hh>
#include <vull/support/Vector.hh>

#include <stdint.h>

namespace vull {

void World::deserialise(PackReader &pack_reader) {
    const auto entity_count = pack_reader.read_varint();
    for (uint32_t i = 0; i < entity_count; i++) {
        const auto component_count = pack_reader.read_varint();
        Entity entity(this, i);
        m_entities.ensure_size(i + 1);
        m_entities[i] = i;
        for (uint32_t j = 0; j < component_count; j++) {
            auto &set = m_component_sets[pack_reader.read_varint()];
            auto *ptr = set.raw_push(entity_index(entity));
            pack_reader.read({ptr, set.object_size()});
        }
    }
}

float World::serialise(PackWriter &pack_writer) {
    pack_writer.start_entry(PackEntryType::WorldData, true);
    pack_writer.write_varint(m_entities.size());
    for (auto id : m_entities) {
        const auto index = entity_index(id);
        uint8_t component_count = 0;
        for (const auto &set : m_component_sets) {
            if (set.contains(index)) {
                component_count++;
            }
        }
        pack_writer.write_varint(component_count);
        for (uint32_t i = 0; i < m_component_sets.size(); i++) {
            auto &set = m_component_sets[i];
            if (!set.contains(index)) {
                continue;
            }
            pack_writer.write_varint(i);
            pack_writer.write({set.raw_at(index), set.object_size()});
        }
    }
    return pack_writer.end_entry();
}

} // namespace vull
