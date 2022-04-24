#pragma once

#include <stdint.h>

/*
 * struct Header {
 *     u8 magic[] {'V', 'P', 'A', 'K'};
 *     Entry entries[]; // until EOF
 * };
 *
 * struct Entry {
 *     u1 compressed;
 *     PackEntryType(u7) type;
 *     u32 size; // uncompressed size in bytes
 *     u8 data[];
 * };
 *
 * struct VertexData(type: 0) {
 *     Vertex vertices[size / sizeof(Vertex)];
 * };
 *
 * struct IndexData(type: 1) {
 *     u32 indices[size / sizeof(u32)];
 * };
 *
 * struct ImageData(type: 2) {
 *     PackImageFormat(u8) format;
 *     varint width;
 *     varint height;
 *     varint mip_count;
 *     u8 mip_data[];
 * };
 *
 * // Handled in World::serialise and World::deserialise
 * struct WorldData(type: 3) {
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
    ImageData = 2,
    WorldData = 3,
};

enum class PackImageFormat : uint8_t {
    Bc1Srgb = 0,
    Bc3Srgb = 1,
    Bc5Unorm = 2,
    RgUnorm = 3,
    RgbaUnorm = 4,
};

} // namespace vull
