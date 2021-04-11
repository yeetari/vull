#pragma once

#include <vull/core/ComponentStorage.hh>
#include <vull/support/Box.hh>
#include <vull/support/Vector.hh>

#include <cstdint>
#include <tuple>
#include <utility>

class EntityManager;
using EntityId = std::size_t;

class Entity {
    const EntityId m_id;
    EntityManager *const m_manager;

public:
    constexpr Entity(EntityId id, EntityManager *manager) : m_id(id), m_manager(manager) {}

    template <typename C, typename... Args>
    C *add(Args &&... args);

    template <typename C>
    C *get() const;

    template <typename C>
    bool has() const;

    template <typename C, typename D, typename... Comps>
    bool has() const;

    template <typename C>
    void remove();

    void destroy();

    auto operator<=>(const Entity &) const = default;
};

template <typename... Comps>
class EntityIterator {
    EntityManager *const m_manager;
    EntityId m_id;

public:
    constexpr EntityIterator(EntityManager *manager, EntityId id);

    constexpr EntityIterator &operator++();

    constexpr auto operator<=>(const EntityIterator &other) const = default;
    constexpr std::tuple<Entity, Comps *...> operator*() const;
};

template <typename... Comps>
class EntityView {
    EntityManager *const m_manager;

public:
    constexpr EntityView(EntityManager *manager) : m_manager(manager) {}

    EntityIterator<Comps...> begin() const;
    EntityIterator<Comps...> end() const;
};

class EntityManager {
    using component_family_type = std::size_t;
    static component_family_type s_component_family_counter; // NOLINT

    template <typename C>
    struct ComponentFamily {
        static component_family_type family() {
            static component_family_type family = s_component_family_counter++;
            return family;
        }
    };

private:
    Vector<Box<ComponentStorage<>>> m_components;
    EntityId m_count{0};
    EntityId m_counter{0};

public:
    EntityManager() {
        for (std::uint32_t i = 0; i < 16; i++) {
            m_components.emplace(nullptr);
        }
    }

    template <typename C, typename... Args>
    C *add_component(EntityId id, Args &&... args);

    template <typename C>
    C *get_component(EntityId id);

    template <typename C>
    void remove_component(EntityId id);

    template <typename... Comps>
    EntityView<Comps...> view();

    Entity create_entity();
    void destroy_entity(EntityId id);

    EntityId entity_count() const { return m_count; }
};

template <typename C, typename... Args>
C *Entity::add(Args &&... args) {
    return m_manager->add_component<C>(m_id, std::forward<Args>(args)...);
}

template <typename C>
C *Entity::get() const {
    return m_manager->get_component<C>(m_id);
}

template <typename C>
bool Entity::has() const {
    return get<C>() != nullptr;
}

template <typename C, typename D, typename... Comps>
bool Entity::has() const {
    return has<C>() && has<D, Comps...>();
}

template <typename C>
void Entity::remove() {
    m_manager->remove_component<C>(m_id);
}

template <typename... Comps>
constexpr EntityIterator<Comps...>::EntityIterator(EntityManager *manager, EntityId id) : m_manager(manager), m_id(id) {
    while (m_id != m_manager->entity_count() && !Entity(m_id, m_manager).has<Comps...>()) {
        ++m_id;
    }
}

template <typename... Comps>
constexpr EntityIterator<Comps...> &EntityIterator<Comps...>::operator++() {
    do {
        ++m_id;
    } while (m_id != m_manager->entity_count() && !Entity(m_id, m_manager).has<Comps...>());
    return *this;
}

template <typename... Comps>
constexpr std::tuple<Entity, Comps *...> EntityIterator<Comps...>::operator*() const {
    return std::make_tuple(Entity(m_id, m_manager), m_manager->get_component<Comps>(m_id)...);
}

template <typename... Comps>
EntityIterator<Comps...> EntityView<Comps...>::begin() const {
    return {m_manager, 0};
}

template <typename... Comps>
EntityIterator<Comps...> EntityView<Comps...>::end() const {
    return {m_manager, m_manager->entity_count()};
}

template <typename C, typename... Args>
C *EntityManager::add_component(EntityId id, Args &&... args) {
    const auto family = ComponentFamily<C>::family();
    if (!m_components[family]) {
        m_components[family] = Box<ComponentStorage<>>::create(sizeof(C));
    }
    m_components[family]->ensure_capacity(id + 1);
    m_components[family]->obtain(id);
    new (m_components[family]->template at<C>(id)) C(std::forward<Args>(args)...);
    return m_components[family]->template at<C>(id);
}

template <typename C>
C *EntityManager::get_component(EntityId id) {
    const auto family = ComponentFamily<C>::family();
    return m_components[family] ? m_components[family]->template at<C>(id) : nullptr;
}

template <typename C>
void EntityManager::remove_component(EntityId id) {
    const auto family = ComponentFamily<C>::family();
    m_components[family]->template at<C>(id)->~C();
    m_components[family]->release(id);
}

template <typename... Comps>
EntityView<Comps...> EntityManager::view() {
    return {this};
}
