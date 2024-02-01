#pragma once

#include <stdint.h>

namespace vull {

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

} // namespace vull
