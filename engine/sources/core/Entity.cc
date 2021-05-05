#include <vull/core/Entity.hh>

#include <vull/core/ComponentStorage.hh>
#include <vull/core/EntityId.hh>
#include <vull/support/Box.hh>
#include <vull/support/Vector.hh>

// NOLINTNEXTLINE
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
        if (components) {
            components->release(id);
        }
    }
}
