#pragma once

#include <stdint.h>

namespace vull::ecs {

using EntityId = uint32_t;
using EntityIndex = uint32_t;
using EntityVersion = uint8_t;

class Entity {
    EntityId m_id;

public:
    static consteval EntityIndex null_index() { return 0xffffffu; }
    static consteval EntityVersion null_version() { return 0xffu; }
    static consteval Entity null() { return make(null_index(), null_version()); }

    static constexpr Entity make(EntityIndex index, EntityVersion version) {
        return EntityId(index) | (EntityId(version) << 24);
    }

    constexpr Entity(EntityId id) : m_id(id) {}
    constexpr Entity() : Entity(null()) {}

    constexpr bool operator==(const Entity &) const = default;
    constexpr EntityIndex index() const { return static_cast<EntityIndex>(m_id & 0xffffffu); }
    constexpr EntityVersion version() const { return static_cast<EntityVersion>((m_id >> 24) & 0xffu); }
};

} // namespace vull::ecs
