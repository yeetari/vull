#include <vull/ecs/entity.hh>

#include <vull/container/vector.hh>
#include <vull/ecs/entity_id.hh>
#include <vull/ecs/sparse_set.hh>
#include <vull/support/assert.hh>
#include <vull/support/utility.hh>

namespace vull {

constexpr auto k_reserved_index = entity_index(~EntityId(0));

EntityManager::EntityManager() : m_free_head(k_reserved_index) {}

Entity EntityManager::create_entity() {
    if (const auto index = m_free_head; index != k_reserved_index) {
        VULL_ASSERT(entity_index(index) == index);
        m_free_head = entity_index(m_entities[index]);
        m_entities[index] = entity_id(index, entity_version(m_entities[index]));
        return {this, m_entities[index]};
    }
    // No IDs available to recycle, generate a new one.
    const auto next_index = m_entities.emplace(m_entities.size());
    VULL_ASSERT(entity_index(next_index) == next_index);
    return {this, next_index};
}

void EntityManager::destroy_entity(EntityId id) {
    const auto index = entity_index(id);
    for (auto &set : m_component_sets) {
        if (set.contains(index)) {
            set.remove(index);
        }
    }
    m_entities[index] = entity_id(m_free_head, entity_version(id) + 1);
    m_free_head = index;
}

bool EntityManager::valid(EntityId id) const {
    return id < m_entities.size() && m_entities[entity_index(id)] == id;
}

} // namespace vull
