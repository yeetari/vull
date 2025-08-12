#include <vull/ecs2/world.hh>

#include <vull/container/vector.hh>
#include <vull/ecs2/entity.hh>
#include <vull/support/assert.hh>
#include <vull/support/utility.hh>

namespace vull::ecs {

Entity World::create() {
    if (m_free_head == Entity::null_index()) {
        // There are no entity ids available to recycle, so make a new one.
        return m_entity_list.emplace(m_entity_list.size());
    }

    const auto index = m_free_head;
    m_free_head = m_entity_list[index].index();
    return (m_entity_list[index] = Entity::make(index, m_entity_list[index].version()));
}

void World::destroy(Entity entity) {
    VULL_ASSERT(is_valid(entity));
    const auto index = entity.index();
    if (entity.version() == Entity::null_version()) {
        // Version limit reached, retire this index.
        m_entity_list[index] = Entity::null();
        return;
    }
    m_entity_list[index] = Entity::make(m_free_head, entity.version() + 1);
    m_free_head = index;
}

bool World::is_valid(Entity entity) const {
    const auto index = entity.index();
    return index < m_entity_list.size() && m_entity_list[index] == entity;
}

} // namespace vull::ecs
