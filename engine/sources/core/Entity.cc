#include <vull/core/Entity.hh>

EntityManager::component_family_type EntityManager::s_component_family_counter = 0;

void Entity::destroy() {
    m_manager->destroy_entity(m_id);
}

Entity EntityManager::create_entity() {
    ++m_count;
    return {m_counter++, this};
}

void EntityManager::destroy_entity(EntityId id) {
    --m_count;
    for (auto &components : m_components) {
        components->release(id);
    }
}
