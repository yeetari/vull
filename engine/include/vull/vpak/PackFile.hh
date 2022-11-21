#pragma once

#include <vull/support/String.hh>

#include <stdint.h>

/*
 * struct {
 *     u8 magic[] {'V', 'P', 'A', 'K'};
 *     u32 entry_count;
 *     u64 entry_table_offset;
 *     u8 block_data[];
 *     EntryTable entry_table;
 * };
 *
 * struct EntryHeader {
 *     EntryType(u8) type;
 *     u8 name_length;
 *     u8 name[name_length];
 *     varint size; // uncompressed size in bytes
 *     varint first_block;
 * };
 *
 * struct EntryTable {
 *     u32 hash_seeds[entry_count];
 *     EntryHeader entries[entry_count];
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
 *     ImageFormat(u8) format;
 *     SamplerKind(u8) sampler_kind;
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

namespace vull::vpak {

enum class EntryType : uint8_t {
    VertexData = 0,
    IndexData = 1,
    ImageData = 2,
    WorldData = 3,
};

enum class ImageFormat : uint8_t {
    Bc1Srgb = 0,
    Bc3Srgba = 1,
    Bc5Unorm = 2,
    RgUnorm = 3,
    RgbaUnorm = 4,
};

enum class SamplerKind : uint8_t {
    LinearRepeat,
    NearestRepeat,
};

// Struct to represent an entry in memory, note not the same representation on disk.
struct Entry {
    String name;
    uint64_t first_block;
    uint32_t size;
    EntryType type;
};

} // namespace vull::vpak
