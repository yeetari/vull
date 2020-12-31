#include <vull/core/Entity.hh>

EntityManager::BaseComponentContainer::family_type EntityManager::BaseComponentContainer::s_family_counter = 0;

void Entity::destroy() {
    m_manager->destroy_entity(m_id);
}

Entity EntityManager::create_entity() {
    m_components[m_count];
    return {m_count++, this};
}

void EntityManager::destroy_entity(EntityId id) {
    m_components.erase(m_components.find(id));
}
