#pragma once

#include <stdint.h>

/*
 * struct Header {
 *     u8 magic[] {'V', 'P', 'A', 'K'};
 *     Entry entries[]; // until EOF
 * };
 *
 * struct Entry {
 *     u8 type;
 *     u32 size; // uncompressed size in bytes
 *     u8 data[];
 * };
 *
 * struct VertexData(type: 0, compressed: true) {
 *     Vertex vertices[size / sizeof(Vertex)];
 * };
 *
 * struct IndexData(type: 1, compressed: size > 24) {
 *     uint32_t indices[size / sizeof(uint32_t)];
 * };
 *
 * // Handled in World::serialise and World::deserialise
 * struct WorldData(type: 2, compressed: true) {
 *     struct Component {
 *         varint component_id;
 *         u8 serialised[];
 *     };
 *     struct Entity {
 *         varint component_count;
 *         Component components[component_count];
 *     };
 *     varint entity_count;
 *     Entity entities[entity_count];
 * };
 */

namespace vull {

enum class PackEntryType : uint8_t {
    VertexData = 0,
    IndexData = 1,
    WorldData = 2,
};

constexpr bool should_compress(PackEntryType type, uint32_t size) {
    return type != PackEntryType::IndexData || size > 24;
}

} // namespace vull
