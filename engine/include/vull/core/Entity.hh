#pragma once

#include <vull/support/Assert.hh>
#include <vull/support/Log.hh>

#include <cstdint>
#include <memory>
#include <unordered_map>
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
};

template <typename... Comps>
class EntityIterator {
    EntityManager *const m_manager;
    EntityId m_id;

public:
    constexpr EntityIterator(EntityManager *manager, EntityId id);

    constexpr EntityIterator &operator++();

    constexpr auto operator<=>(const EntityIterator &other) const = default;
    constexpr Entity operator*() const { return Entity(m_id, m_manager); }
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
    struct BaseComponentContainer {
        using family_type = std::size_t;

    public:
        // NOLINTNEXTLINE
        static family_type s_family_counter;

        constexpr BaseComponentContainer() = default;
        BaseComponentContainer(const BaseComponentContainer &) = delete;
        BaseComponentContainer(BaseComponentContainer &&) = delete;
        virtual ~BaseComponentContainer() = default;

        BaseComponentContainer &operator=(const BaseComponentContainer &) = delete;
        BaseComponentContainer &operator=(BaseComponentContainer &&) = delete;
    };

    template <typename C>
    struct ComponentContainer : public BaseComponentContainer {
        static family_type family() {
            static family_type family = s_family_counter++;
            return family;
        }

    private:
        C m_comp;

    public:
        template <typename... Args>
        constexpr explicit ComponentContainer(Args &&... args) : m_comp(std::forward<Args>(args)...) {}

        C *comp() { return &m_comp; }
    };

private:
    mutable std::unordered_map<
        EntityId, std::unordered_map<BaseComponentContainer::family_type, std::unique_ptr<BaseComponentContainer>>>
        m_components;
    EntityId m_count{0};

public:
    template <typename C, typename... Args>
    C *add_component(EntityId id, Args &&... args);

    template <typename C>
    C *get_component(EntityId id) const;

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
EntityIterator<Comps...> EntityView<Comps...>::begin() const {
    return {m_manager, 0};
}

template <typename... Comps>
EntityIterator<Comps...> EntityView<Comps...>::end() const {
    return {m_manager, m_manager->entity_count()};
}

template <typename C, typename... Args>
C *EntityManager::add_component(EntityId id, Args &&... args) {
    auto &entity_components = m_components[id];
    ASSERT(!entity_components.contains(ComponentContainer<C>::family()));
    auto pair = entity_components.emplace(ComponentContainer<C>::family(),
                                          std::make_unique<ComponentContainer<C>>(std::forward<Args>(args)...));
    return static_cast<ComponentContainer<C> *>(pair.first->second.get())->comp();
}

template <typename C>
C *EntityManager::get_component(EntityId id) const {
    const auto &entity_components = m_components[id];
    const auto it = entity_components.find(ComponentContainer<C>::family());
    return it != entity_components.end() ? static_cast<ComponentContainer<C> *>(it->second.get())->comp() : nullptr;
}

template <typename C>
void EntityManager::remove_component(EntityId id) {
    ASSERT(m_components.contains(id));
    auto &entity_components = m_components.at(id);
    entity_components.erase(id);
}

template <typename... Comps>
EntityView<Comps...> EntityManager::view() {
    return {this};
}
