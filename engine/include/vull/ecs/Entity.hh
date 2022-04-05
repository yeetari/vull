#pragma once

#include <vull/ecs/SparseSet.hh>
#include <vull/support/Tuple.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>

#include <stdint.h>

namespace vull {

class EntityManager;
using EntityId = uint32_t;

constexpr EntityId entity_id(auto index, auto version) {
    return EntityId(index) | (EntityId(version) << 24u);
}
constexpr EntityId entity_index(EntityId id) {
    return id & 0xffffffu;
}
constexpr EntityId entity_version(EntityId id) {
    return id >> 24u;
}

class Entity {
    EntityManager *const m_manager;
    const EntityId m_id;

public:
    constexpr Entity(EntityManager *manager, EntityId id) : m_manager(manager), m_id(id) {}

    template <typename C, typename... Args>
    void add(Args &&...args);
    template <typename C>
    C &get();
    template <typename... Comps>
    bool has();
    template <typename C>
    void remove();

    void destroy();
    operator EntityId() const { return m_id; }
};

template <typename... Comps>
class EntityIterator;

template <typename... Comps>
class EntityView;

template <typename C>
class EntityIterator<C> {
    template <typename... Comps>
    friend class EntityView;

protected:
    EntityManager *const m_manager;
    EntityId *m_current_id;
    C *m_current_component;

    EntityIterator(EntityManager *manager, EntityId *current_id, C *current_component)
        : m_manager(manager), m_current_id(current_id), m_current_component(current_component) {}

public:
    bool operator!=(const EntityIterator &) const;
    EntityIterator &operator++();
    Tuple<Entity, C &> operator*() const;
};

template <typename C, typename... Comps>
class EntityIterator<C, Comps...> : public EntityIterator<C> {
public:
    using EntityIterator<C>::EntityIterator;
    EntityIterator &operator++();
    Tuple<Entity, C &, Comps &...> operator*() const;
};

template <typename C, typename... Comps>
class EntityView<C, Comps...> {
    friend EntityManager;

private:
    EntityManager *const m_manager;
    SparseSet<EntityId> &m_component_set;

    EntityView(EntityManager *manager);

public:
    EntityIterator<C, Comps...> begin() const;
    EntityIterator<C, Comps...> end() const;
};

class EntityManager {
    template <typename... Comps>
    friend class EntityView;

private:
    Vector<SparseSet<EntityId>> m_component_sets;
    Vector<EntityId, EntityId> m_entities;
    EntityId m_free_head;

public:
    EntityManager();
    EntityManager(const EntityManager &) = delete;
    EntityManager(EntityManager &&) = delete;
    ~EntityManager() = default;

    EntityManager &operator=(const EntityManager &) = delete;
    EntityManager &operator=(EntityManager &&) = delete;

    template <typename C>
    void register_component();

    template <typename C, typename... Args>
    void add_component(EntityId id, Args &&...args);
    template <typename C>
    C &get_component(EntityId id);
    template <typename C>
    bool has_component(EntityId id);
    template <typename C, typename D, typename... Comps>
    bool has_component(EntityId id);
    template <typename C>
    void remove_component(EntityId id);

    Entity create_entity();
    void destroy_entity(EntityId id);
    bool valid(EntityId id) const;
    template <typename... Comps>
    EntityView<Comps...> view();
};

template <typename C, typename... Args>
void Entity::add(Args &&...args) {
    m_manager->add_component<C>(m_id, forward<Args>(args)...);
}

template <typename C>
C &Entity::get() {
    return m_manager->get_component<C>(m_id);
}

template <typename... Comps>
bool Entity::has() {
    return m_manager->has_component<Comps...>(m_id);
}

template <typename C>
void Entity::remove() {
    m_manager->remove_component<C>(m_id);
}

inline void Entity::destroy() {
    m_manager->destroy_entity(m_id);
}

template <typename C>
bool EntityIterator<C>::operator!=(const EntityIterator &other) const {
    return m_current_id != other.m_current_id;
}

template <typename C>
EntityIterator<C> &EntityIterator<C>::operator++() {
    m_current_component++;
    m_current_id++;
    return *this;
}

template <typename C>
Tuple<Entity, C &> EntityIterator<C>::operator*() const {
    return {Entity(m_manager, *m_current_id), *m_current_component};
}

template <typename C, typename... Comps>
EntityIterator<C, Comps...> &EntityIterator<C, Comps...>::operator++() {
    do {
        EntityIterator<C>::operator++();
    } while (EntityIterator<C>::m_manager->valid(*EntityIterator<C>::m_current_id) &&
             !EntityIterator<C>::m_manager->template has_component<Comps...>(*EntityIterator<C>::m_current_id));
    return *this;
}

template <typename C, typename... Comps>
Tuple<Entity, C &, Comps &...> EntityIterator<C, Comps...>::operator*() const {
    return {Entity(EntityIterator<C>::m_manager, *EntityIterator<C>::m_current_id),
            *EntityIterator<C>::m_current_component,
            EntityIterator<C>::m_manager->template get_component<Comps>(*EntityIterator<C>::m_current_id)...};
}

template <typename C, typename... Comps>
EntityView<C, Comps...>::EntityView(EntityManager *manager)
    : m_manager(manager), m_component_set(manager->m_component_sets[C::k_component_id]) {}

template <typename C, typename... Comps>
EntityIterator<C, Comps...> EntityView<C, Comps...>::begin() const {
    return {m_manager, m_component_set.dense_begin(), m_component_set.storage_begin<C>()};
}

template <typename C, typename... Comps>
EntityIterator<C, Comps...> EntityView<C, Comps...>::end() const {
    return {m_manager, m_component_set.dense_end(), m_component_set.storage_end<C>()};
}

template <typename C>
void EntityManager::register_component() {
    m_component_sets.ensure_size(C::k_component_id + 1);
    m_component_sets[C::k_component_id].template initialise<C>();
}

template <typename C, typename... Args>
void EntityManager::add_component(EntityId id, Args &&...args) {
    m_component_sets[C::k_component_id].template emplace<C>(entity_index(id), forward<Args>(args)...);
}

template <typename C>
C &EntityManager::get_component(EntityId id) {
    return m_component_sets[C::k_component_id].template at<C>(entity_index(id));
}

template <typename C>
bool EntityManager::has_component(EntityId id) {
    return m_component_sets[C::k_component_id].contains(entity_index(id));
}

template <typename C, typename D, typename... Comps>
bool EntityManager::has_component(EntityId id) {
    return has_component<C>(id) && has_component<D, Comps...>(id);
}

template <typename C>
void EntityManager::remove_component(EntityId id) {
    m_component_sets[C::k_component_id].remove(entity_index(id));
}

template <typename... Comps>
EntityView<Comps...> EntityManager::view() {
    return {this};
}

} // namespace vull