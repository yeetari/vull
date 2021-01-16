#pragma once

#include <vull/support/Box.hh>
#include <vull/support/Vector.hh>

#include <cstdint>
#include <utility>

struct World;

struct BaseSystem {
    using family_type = std::size_t;

public:
    // NOLINTNEXTLINE
    static family_type s_family_counter;

    constexpr BaseSystem() = default;
    BaseSystem(const BaseSystem &) = delete;
    BaseSystem(BaseSystem &&) = delete;
    virtual ~BaseSystem() = default;

    BaseSystem &operator=(const BaseSystem &) = delete;
    BaseSystem &operator=(BaseSystem &&) = delete;

    virtual void update(World *world, float dt) = 0;
};

template <typename>
struct System : public BaseSystem {
    static family_type family() {
        static family_type family = s_family_counter++;
        return family;
    }
};

class SystemManager {
    Vector<Box<BaseSystem>> m_systems;

public:
    template <typename S, typename... Args>
    void add(Args &&... args);

    template <typename S>
    S *get() const;

    template <typename S>
    void remove();

    const auto &systems() const { return m_systems; }
};

template <typename S, typename... Args>
void SystemManager::add(Args &&... args) {
    for (std::uint32_t i = m_systems.size(); i < S::family() + 1; i++) {
        m_systems.emplace();
    }
    m_systems[S::family()] = Box<S>::create(std::forward<Args>(args)...);
}

template <typename S>
S *SystemManager::get() const {
    return static_cast<S *>(*m_systems[S::family()]);
}

template <typename S>
void SystemManager::remove() {
    // TODO!
    ENSURE_NOT_REACHED();
}
