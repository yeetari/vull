#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <utility>

class World;

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
    std::unordered_map<BaseSystem::family_type, std::unique_ptr<BaseSystem>> m_systems;

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
    m_systems.emplace(S::family(), std::make_unique<S>(std::forward<Args>(args)...));
}

template <typename S>
S *SystemManager::get() const {
    return m_systems.at(S::family()).get();
}

template <typename S>
void SystemManager::remove() {
    m_systems.erase(S::family());
}
