#pragma once

#include <vull/support/string.hh>

#include <stdint.h>

namespace vull::vpak {

constexpr uint64_t k_header_size = 24;
constexpr uint32_t k_magic_number = 0x8186564bu;
constexpr uint32_t k_entry_limit = 1u << 20u;

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

enum class CompressionLevel {
    Fast,
    Normal,
    Ultra,
};

enum class VpakError {
    BadMagic,
    BadVersion,
    BadFlags,
    TooManyEntries,
};

} // namespace vull::vpak
