#include "TextureLoader.hh"

#include <vull/maths/Common.hh>
#include <vull/support/Array.hh>
#include <vull/support/Assert.hh>
#include <vull/support/ScopeGuard.hh>
#include <vull/support/String.hh>
#include <vull/vpak/PackWriter.hh>

#include <stdint.h>
#include <stdio.h>
#include <zstd.h>

#define DWORD_LE(mem, start)                                                                                           \
    (static_cast<uint32_t>((mem)[(start)] << 0u) | static_cast<uint32_t>((mem)[(start) + 1] << 8u) |                   \
     static_cast<uint32_t>((mem)[(start) + 2] << 16u) | static_cast<uint32_t>((mem)[(start) + 3] << 24u))

#define FOUR_CC(str)                                                                                                   \
    ((static_cast<uint32_t>((str)[0] << 0u)) | (static_cast<uint32_t>((str)[1] << 8u)) |                               \
     (static_cast<uint32_t>((str)[2] << 16u)) | (static_cast<uint32_t>((str)[3] << 24u)))

namespace {

bool load_dds(vull::PackWriter &pack_writer, FILE *file) {
    vull::Array<uint8_t, 128> header{};
    if (fread(header.data(), 1, header.size(), file) != header.size()) {
        return false;
    }

    // Check magic number.
    if (DWORD_LE(header, 0) != 0x20534444u) {
        return false;
    }

    // Check header size.
    if (DWORD_LE(header, 4) != 124) {
        return false;
    }

    // Check flags make sense.
    if ((DWORD_LE(header, 8) & 0x1007u) != 0x1007u) {
        return false;
    }

    uint32_t mip_count = (DWORD_LE(header, 8) & 0x20000u) != 0u ? DWORD_LE(header, 28) : 1u;
    pack_writer.start_entry(vull::PackEntryType::ImageData, true);

    uint32_t block_size;
    VULL_ENSURE((DWORD_LE(header, 80) & 0x4u) == 0x4u);
    switch (DWORD_LE(header, 84)) {
    case FOUR_CC("DXT1"):
        block_size = 8;
        pack_writer.write_byte(uint8_t(vull::PackImageFormat::Bc1Srgb));
        break;
    case FOUR_CC("DXT5"):
        block_size = 16;
        pack_writer.write_byte(uint8_t(vull::PackImageFormat::Bc3Srgb));
        break;
    case FOUR_CC("ATI2"):
        block_size = 16;
        pack_writer.write_byte(uint8_t(vull::PackImageFormat::Bc5Unorm));
        break;
    default:
        VULL_ENSURE_NOT_REACHED();
    }

    uint32_t width = DWORD_LE(header, 16);
    uint32_t height = DWORD_LE(header, 12);
    pack_writer.write_varint(width);
    pack_writer.write_varint(height);

    // Loop over mips.
    auto *read_buffer = new uint8_t[ZSTD_CStreamInSize()];
    pack_writer.write_varint(mip_count);
    for (uint32_t i = 0; i < mip_count; i++) {
        const uint32_t mip_size = ((width + 3) / 4) * ((height + 3) / 4) * block_size;
        // TODO: Have this functionality in PackWriter, maybe a write_chunk function? Also how to handle read_buffer?
        for (uint32_t bytes_written = 0; bytes_written < mip_size;) {
            const uint32_t part_size = vull::min(mip_size - bytes_written, static_cast<uint32_t>(ZSTD_CStreamInSize()));
            VULL_ENSURE(fread(read_buffer, 1, part_size, file) == part_size);
            pack_writer.write({read_buffer, part_size});
            bytes_written += part_size;
        }
        width /= 2;
        height /= 2;
    }
    delete[] read_buffer;
    return true;
}

} // namespace

bool load_texture(vull::PackWriter &pack_writer, const vull::String &path) {
    auto *file = fopen(path.data(), "rb");
    if (file == nullptr) {
        return false;
    }
    vull::ScopeGuard close_guard([file] {
        fclose(file);
    });
    return load_dds(pack_writer, file);
}
