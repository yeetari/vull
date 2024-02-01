#pragma once

#include <vull/support/string.hh>

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
 *     v64 name_length;
 *     u8 name[name_length];
 *     v32 size; // uncompressed size in bytes
 *     v64 first_block;
 * };
 *
 * struct EntryTable {
 *     u32 hash_seeds[entry_count];
 *     EntryHeader entries[entry_count];
 * };
 *
 * struct Blob(type: 0) {
 *     u8 data[size];
 * };
 *
 * struct Image(type: 1) {
 *     ImageFormat(u8) format;
 *     ImageFilter(u8) mag_filter;
 *     ImageFilter(u8) min_filter;
 *     ImageWrapMode(u8) wrap_u;
 *     ImageWrapMode(u8) wrap_v;
 *     v32 width;
 *     v32 height;
 *     v32 mip_count;
 *     u8 mip_data[];
 * };
 *
 * // Handled in World::serialise and World::deserialise
 * struct World(type: 2) {
 *     struct ComponentSet {
 *         v32 entity_count;
 *         u8 serialised_data[];
 *         v32 entity_ids[entity_count];
 *     };
 *     v32 entity_count;
 *     v32 set_count;
 *     ComponentSet sets[set_count];
 * };
 */

namespace vull::vpak {

enum class EntryType : uint8_t {
    Blob = 0,
    Image = 1,
    World = 2,
};

enum class ImageFormat : uint8_t {
    Bc1Srgb = 0,
    Bc3Srgba = 1,
    Bc5Unorm = 2,
    RgUnorm = 3,
    RgbaUnorm = 4,
    Bc7Srgb = 5,
};

enum class ImageFilter : uint8_t {
    Nearest,
    Linear,
    NearestMipmapNearest,
    LinearMipmapNearest,
    NearestMipmapLinear,
    LinearMipmapLinear,
};

enum class ImageWrapMode : uint8_t {
    ClampToEdge,
    MirroredRepeat,
    Repeat,
};

// Struct to represent an entry in memory, note not the same representation on disk.
struct Entry {
    String name;
    uint64_t first_block;
    uint32_t size;
    EntryType type;
};

} // namespace vull::vpak
