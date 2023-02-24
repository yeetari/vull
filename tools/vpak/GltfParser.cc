#include "GltfParser.hh"

#include "PngStream.hh"

#include <vull/core/BoundingBox.hh>
#include <vull/core/BoundingSphere.hh>
#include <vull/core/Log.hh>
#include <vull/core/Transform.hh>
#include <vull/ecs/World.hh>
#include <vull/graphics/Material.hh>
#include <vull/graphics/Mesh.hh>
#include <vull/graphics/Vertex.hh>
#include <vull/maths/Vec.hh>
#include <vull/platform/Latch.hh>
#include <vull/platform/Mutex.hh>
#include <vull/platform/Timer.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Atomic.hh>
#include <vull/support/Format.hh>
#include <vull/support/HashMap.hh>
#include <vull/support/PerfectMap.hh>
#include <vull/support/ScopedLock.hh>
#include <vull/support/SpanStream.hh>
#include <vull/support/Vector.hh>
#include <vull/tasklet/Scheduler.hh>
#include <vull/tasklet/Tasklet.hh>
#include <vull/vpak/PackFile.hh>
#include <vull/vpak/Writer.hh>

#define STB_DXT_IMPLEMENTATION
#define STBD_FABS vull::abs
#include <linux/futex.h>
#include <meshoptimizer.h>
#include <simdjson.h>
#include <stb_dxt.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>

#define DWORD_LE(data, start)                                                                                          \
    (static_cast<uint32_t>((data)[(start)] << 0u) | static_cast<uint32_t>((data)[(start) + 1] << 8u) |                 \
     static_cast<uint32_t>((data)[(start) + 2] << 16u) | static_cast<uint32_t>((data)[(start) + 3] << 24u))

#define EXPECT_SUCCESS(expr, msg, ...)                                                                                 \
    if ((expr) != simdjson::SUCCESS) {                                                                                 \
        vull::error("[gltf] " msg __VA_OPT__(, ) __VA_ARGS__);                                                         \
        return false;                                                                                                  \
    }

namespace vull {
namespace {

enum class TextureType {
    Albedo,
    Normal,
};

class Converter {
    const uint8_t *const m_binary_blob;
    vpak::Writer &m_pack_writer;
    simdjson::dom::element &m_document;
    const bool m_max_resolution;

    HashMap<uint64_t, String> m_albedo_paths;
    HashMap<uint64_t, String> m_normal_paths;
    Mutex m_material_map_mutex;

    struct MeshBounds {
        BoundingBox box;
        BoundingSphere sphere;
    };
    HashMap<String, MeshBounds> m_mesh_bounds;
    Mutex m_mesh_bounds_mutex;

    Material make_material(simdjson::simdjson_result<simdjson::dom::element> primitive);

public:
    Converter(const uint8_t *binary_blob, vpak::Writer &pack_writer, simdjson::dom::element &document,
              bool max_resolution)
        : m_binary_blob(binary_blob), m_pack_writer(pack_writer), m_document(document),
          m_max_resolution(max_resolution) {}

    bool convert(Latch &latch);
    void process_texture(uint64_t index, String &path, String desired_path, TextureType type);
    bool process_material(const simdjson::dom::element &material, uint64_t index);
    bool process_primitive(const simdjson::dom::object &primitive, String &&name);
    bool visit_node(World &world, uint64_t index, EntityId parent_id);
};

bool Converter::convert(Latch &latch) {
    // Emit default albedo texture.
    auto albedo_entry = m_pack_writer.start_entry("/default_albedo", vpak::EntryType::Image);
    VULL_EXPECT(albedo_entry.write_byte(uint8_t(vpak::ImageFormat::RgbaUnorm)));
    VULL_EXPECT(albedo_entry.write_byte(uint8_t(vpak::SamplerKind::NearestRepeat)));
    VULL_EXPECT(albedo_entry.write_varint(16u));
    VULL_EXPECT(albedo_entry.write_varint(16u));
    VULL_EXPECT(albedo_entry.write_varint(1u));
    constexpr Array colours{
        Vec<uint8_t, 4>(0xff, 0x69, 0xb4, 0xff),
        Vec<uint8_t, 4>(0x94, 0x00, 0xd3, 0xff),
    };
    for (uint32_t y = 0; y < 16; y++) {
        for (uint32_t x = 0; x < 16; x++) {
            uint32_t colour_index = (x + y) % colours.size();
            VULL_EXPECT(albedo_entry.write({&colours[colour_index], 4}));
        }
    }
    albedo_entry.finish();

    // Emit default normal map texture.
    auto normal_entry = m_pack_writer.start_entry("/default_normal", vpak::EntryType::Image);
    VULL_EXPECT(normal_entry.write_byte(uint8_t(vpak::ImageFormat::RgUnorm)));
    VULL_EXPECT(normal_entry.write_byte(uint8_t(vpak::SamplerKind::LinearRepeat)));
    VULL_EXPECT(normal_entry.write(Array<uint8_t, 5>{1u, 1u, 1u, 127u, 127u}.span()));
    normal_entry.finish();

    // Process meshes.
    simdjson::dom::array meshes;
    EXPECT_SUCCESS(m_document["meshes"].get(meshes), "Missing \"meshes\" property")

    simdjson::dom::array materials;
    EXPECT_SUCCESS(m_document["materials"].get(materials), "Missing \"materials\" property")

    latch.increment(static_cast<uint32_t>(materials.size()));
    for (auto mesh : meshes) {
        simdjson::dom::array primitives;
        EXPECT_SUCCESS(mesh["primitives"].get(primitives), "Missing \"primitives\" property")
        latch.increment(static_cast<uint32_t>(primitives.size()));
    }

    // Initial value of 1 can now safely be decremented.
    latch.count_down();

    for (uint64_t i = 0; auto material : materials) {
        vull::schedule([this, material, &latch, index = i++] {
            process_material(material, index);
            latch.count_down();
        });
    }

    for (auto mesh : meshes) {
        std::string_view mesh_name_;
        EXPECT_SUCCESS(mesh["name"].get(mesh_name_), "Missing mesh name")
        StringView mesh_name(mesh_name_.data(), mesh_name_.length());

        simdjson::dom::array primitives;
        VULL_IGNORE(mesh["primitives"].get(primitives));
        for (uint64_t i = 0; i < primitives.size(); i++) {
            simdjson::dom::object primitive;
            EXPECT_SUCCESS(primitives.at(i).get(primitive), "Element in \"primitives\" array is not an object")
            vull::schedule([this, primitive, &latch, name = vull::format("{}.{}", mesh_name, i)]() mutable {
                process_primitive(primitive, vull::move(name));
                latch.count_down();
            });
        }
    }
    return true;
}

void Converter::process_texture(uint64_t index, String &path, String desired_path, TextureType type) {
    simdjson::dom::object texture;
    if (auto error = m_document["textures"].at(index).get(texture)) {
        vull::error("[gltf] Failed to get texture at index {}: {}", index, simdjson::error_message(error));
        return;
    }

    uint64_t image_index;
    if (texture["source"].get(image_index) != simdjson::SUCCESS) {
        // No source texture, use default error texture.
        return;
    }

    simdjson::dom::object image;
    if (auto error = m_document["images"].at(image_index).get(image)) {
        vull::error("[gltf] Failed to get image at index {}: {}", image_index, simdjson::error_message(error));
        return;
    }

    std::string_view image_name_ = "?";
    VULL_IGNORE(image["name"].get(image_name_));
    StringView image_name(image_name_.data(), image_name_.length());

    // Default to linear filtering and repeat wrapping.
    uint64_t mag_filter = 9729;
    uint64_t min_filter = 9729;
    uint64_t wrap_s = 10497;
    uint64_t wrap_t = 10497;
    if (uint64_t sampler_index; texture["sampler"].get(sampler_index) == simdjson::SUCCESS) {
        simdjson::dom::object sampler;
        if (auto error = m_document["samplers"].at(sampler_index).get(sampler)) {
            vull::error("[gltf] Failed to get sampler at index {}: {}", sampler_index, simdjson::error_message(error));
            return;
        }
        VULL_IGNORE(sampler["magFilter"].get(mag_filter));
        VULL_IGNORE(sampler["minFilter"].get(min_filter));
        VULL_IGNORE(sampler["wrapS"].get(wrap_s));
        VULL_IGNORE(sampler["wrapT"].get(wrap_t));
    }

    if (wrap_s != wrap_t) {
        // TODO: Implement.
        vull::warn("[gltf] Image '{}' has a differing S and T wrapping mode, which is unsupported", image_name);
        return;
    }

    if (wrap_s != 10497) {
        // TODO: Implement.
        vull::warn("[gltf] Ignoring non-repeat wrapping mode for image '{}'", image_name);
    }

    // TODO: Look at min_filter.
    const auto sampler_kind = mag_filter == 9728 ? vpak::SamplerKind::NearestRepeat : vpak::SamplerKind::LinearRepeat;

    uint64_t buffer_view_index;
    if (image["bufferView"].get(buffer_view_index) != simdjson::SUCCESS) {
        vull::warn("[gltf] Couldn't load image '{}', data not stored in GLB", image_name);
        return;
    }

    std::string_view mime_type;
    if (image["mimeType"].get(mime_type) != simdjson::SUCCESS) {
        vull::error("[gltf] Image '{}' missing mime type", image_name);
        return;
    }

    if (mime_type != "image/png") {
        vull::warn("[gltf] Image '{}' has unsupported mime type {}", image_name,
                   StringView(mime_type.data(), mime_type.length()));
        return;
    }

    simdjson::dom::object buffer_view;
    if (auto error = m_document["bufferViews"].at(buffer_view_index).get(buffer_view)) {
        vull::error("[gltf] Failed to get buffer view at index {}: {}", buffer_view_index,
                    simdjson::error_message(error));
        return;
    }

    uint64_t byte_offset = 0;
    VULL_IGNORE(buffer_view["byteOffset"].get(byte_offset));

    uint64_t byte_length;
    if (buffer_view["byteLength"].get(byte_length) != simdjson::SUCCESS) {
        vull::error("[gltf] Missing byte length");
        return;
    }

    if (byte_length > 0x100000000) {
        vull::error("[gltf] Image '{}' larger than 4 GiB", image_name);
        return;
    }

    auto span_stream =
        vull::make_unique<SpanStream>(vull::make_span(&m_binary_blob[byte_offset], static_cast<uint32_t>(byte_length)));
    auto png_stream_or_error = PngStream::create(vull::move(span_stream));
    if (png_stream_or_error.is_error()) {
        vull::error("[gltf] Failed to load image '{}'", image_name);
        return;
    }
    auto png_stream = png_stream_or_error.disown_value();

    const auto pixel_byte_count = png_stream.pixel_byte_count();
    if (pixel_byte_count != 3 && pixel_byte_count != 4) {
        vull::error("[gltf] Image '{}' has a pixel byte count of {}", image_name, pixel_byte_count);
        return;
    }

    vpak::ImageFormat format;
    switch (type) {
    case TextureType::Albedo:
        format = png_stream.pixel_byte_count() == 4 ? vpak::ImageFormat::Bc3Srgba : vpak::ImageFormat::Bc1Srgb;
        break;
    case TextureType::Normal:
        format = vpak::ImageFormat::Bc5Unorm;
        break;
    }

    auto width = png_stream.width();
    auto height = png_stream.height();
    const auto row_byte_count = png_stream.row_byte_count();
    const auto mip_count = vull::log2(vull::max(width, height)) + 1u;
    Vector<Vector<uint8_t>> mip_buffers(mip_count);

    mip_buffers[0].resize_unsafe(width * height * pixel_byte_count);
    for (uint32_t y = 0; y < height; y++) {
        png_stream.read_row(mip_buffers[0].span().subspan(y * row_byte_count));
    }

    Vec2u source_size(width, height);
    for (uint32_t i = 1; i < mip_count; i++) {
        const auto mip_size = source_size >> 1;
        mip_buffers[i].resize_unsafe(mip_size.x() * mip_size.y() * pixel_byte_count);

        const auto scale = Vec2f(1.0f) / vull::max(mip_size - 1, Vec2u(1u));
        const auto *source = mip_buffers[i - 1].data();
        for (uint32_t y = 0; y < mip_size.y(); y++) {
            for (uint32_t x = 0; x < mip_size.x(); x++) {
                const auto sample_position = Vec2f(x, y) * scale;
                const auto scaled_coord = sample_position * (source_size - 1);
                const auto scaled_floor = vull::floor(scaled_coord);
                const auto scaled_ceil = vull::ceil(scaled_coord);
                const auto lerp_factor = scaled_coord - scaled_floor;

                const auto sample_source = [&](const Vec2u &position) {
                    const auto *pixel = &source[(position.y() * source_size.x() + position.x()) * pixel_byte_count];
                    Vec4f texel(pixel[0], pixel[1], pixel[2], 0.0f);
                    if (pixel_byte_count == 4) {
                        texel[3] = pixel[3];
                    }
                    return texel;
                };
                const auto t0 = sample_source(scaled_floor);
                const auto t1 = sample_source({scaled_ceil.x(), scaled_floor.y()});
                const auto t2 = sample_source(scaled_ceil);
                const auto t3 = sample_source({scaled_floor.x(), scaled_ceil.y()});

                const auto l0 = vull::lerp(t0, t1, lerp_factor.x());
                const auto l1 = vull::lerp(t2, t3, lerp_factor.x());
                const auto l2 = vull::lerp(l0, l1, lerp_factor.y());

                auto *pixel = &mip_buffers[i][(y * mip_size.x() + x) * pixel_byte_count];
                pixel[0] = static_cast<uint8_t>(l2[0]);
                pixel[1] = static_cast<uint8_t>(l2[1]);
                pixel[2] = static_cast<uint8_t>(l2[2]);
                if (pixel_byte_count == 4) {
                    pixel[3] = static_cast<uint8_t>(l2[3]);
                }
            }
        }
        source_size >>= 1;
    }

    constexpr uint32_t log_threshold_resolution = 11u;
    uint32_t mip_offset = 0;
    if (!m_max_resolution && width == height && mip_count > log_threshold_resolution) {
        // Drop first mip.
        width >>= 1;
        height >>= 1;
        mip_offset++;
    }

    auto entry = m_pack_writer.start_entry(path = vull::move(desired_path), vpak::EntryType::Image);
    VULL_EXPECT(entry.write_byte(uint8_t(format)));
    VULL_EXPECT(entry.write_byte(uint8_t(sampler_kind)));
    VULL_EXPECT(entry.write_varint(width));
    VULL_EXPECT(entry.write_varint(height));
    VULL_EXPECT(entry.write_varint(mip_count - mip_offset));

    for (uint32_t i = mip_offset; i < mip_count; i++) {
        for (uint32_t block_y = 0; block_y < height; block_y += 4) {
            for (uint32_t block_x = 0; block_x < width; block_x += 4) {
                // TODO: These cases can probably be unified into one path.
                switch (format) {
                case vpak::ImageFormat::Bc1Srgb: {
                    // 64-bit compressed block.
                    Array<uint8_t, 8> compressed_block;

                    // 64-byte (4x4 * 4 bytes per pixel (alpha padded)) input.
                    Array<uint8_t, 64> source_block{};

                    // = 16 bytes per row.
                    for (uint32_t y = 0; y < 4 && block_y + y < height; y++) {
                        const auto *row = &mip_buffers[i][(block_y + y) * width * pixel_byte_count];
                        for (uint32_t x = 0; x < 4 && block_x + x < width; x++) {
                            memcpy(&source_block[y * 16 + x * 4], &row[(block_x + x) * pixel_byte_count], 3);
                        }
                    }
                    stb_compress_dxt_block(compressed_block.data(), source_block.data(), 0, STB_DXT_HIGHQUAL);
                    VULL_EXPECT(entry.write(compressed_block.span()));
                    break;
                }
                case vpak::ImageFormat::Bc3Srgba: {
                    // 128-bit compressed block.
                    Array<uint8_t, 16> compressed_block;

                    // 64-byte (4x4 * 4 bytes per pixel) input.
                    Array<uint8_t, 64> source_block{};

                    // = 16 bytes per row.
                    for (uint32_t y = 0; y < 4 && block_y + y < height; y++) {
                        const auto *row = &mip_buffers[i][(block_y + y) * width * pixel_byte_count];
                        for (uint32_t x = 0; x < 4 && block_x + x < width; x++) {
                            memcpy(&source_block[y * 16 + x * 4], &row[(block_x + x) * pixel_byte_count], 4);
                        }
                    }
                    stb_compress_dxt_block(compressed_block.data(), source_block.data(), 1, STB_DXT_HIGHQUAL);
                    VULL_EXPECT(entry.write(compressed_block.span()));
                    break;
                }
                case vpak::ImageFormat::Bc5Unorm: {
                    // 128-bit compressed block.
                    Array<uint8_t, 16> compressed_block;

                    // 32-byte (4x4 * 2 bytes per pixel) input.
                    Array<uint8_t, 32> source_block{};

                    // = 8 bytes per row.
                    for (uint32_t y = 0; y < 4 && block_y + y < height; y++) {
                        const auto *row = &mip_buffers[i][(block_y + y) * width * pixel_byte_count];
                        for (uint32_t x = 0; x < 4 && block_x + x < width; x++) {
                            memcpy(&source_block[y * 8 + x * 2], &row[(block_x + x) * pixel_byte_count], 2);
                        }
                    }
                    stb_compress_bc5_block(compressed_block.data(), source_block.data());
                    VULL_EXPECT(entry.write(compressed_block.span()));
                    break;
                }
                }
            }
        }
        width >>= 1;
        height >>= 1;
    }
    entry.finish();
}

bool Converter::process_material(const simdjson::dom::element &material, uint64_t index) {
    std::string_view name_;
    EXPECT_SUCCESS(material["name"].get(name_), "Missing material name")
    StringView name(name_.data(), name_.length());

    if (material["occlusionTexture"].error() == simdjson::SUCCESS) {
        vull::warn("[gltf] Material '{}' has an occlusion texture, which is unimplemented", name);
    }
    if (material["emissiveTexture"].error() == simdjson::SUCCESS ||
        material["emissiveFactor"].error() == simdjson::SUCCESS) {
        vull::warn("[gltf] Material '{}' has emissive properties, which is unimplemented", name);
    }
    if (material["doubleSided"].error() == simdjson::SUCCESS) {
        vull::warn("[gltf] Material '{}' is double sided, which is unsupported", name);
    }

    std::string_view alpha_mode = "OPAQUE";
    VULL_IGNORE(material["alphaMode"].get(alpha_mode));
    if (alpha_mode != "OPAQUE") {
        StringView mode(alpha_mode.data(), alpha_mode.length());
        vull::warn("[gltf] Material '{}' has unsupported alpha mode {}", name, mode);
    }

    auto pbr_info = material["pbrMetallicRoughness"];
    auto normal_info = material["normalTexture"];

    if (pbr_info["baseColorFactor"].error() == simdjson::SUCCESS) {
        // TODO: If both factors and textures are present, the factor value acts as a linear multiplier for the
        //       corresponding texture values.
        // TODO: In addition to the material properties, if a primitive specifies a vertex color using the attribute
        //       semantic property COLOR_0, then this value acts as an additional linear multiplier to base color.
        vull::warn("[gltf] Ignoring baseColorFactor on material '{}'", name);
    }
    double normal_scale = 1.0;
    VULL_IGNORE(normal_info["scale"].get(normal_scale));
    if (normal_scale != 1.0) {
        vull::warn("[gltf] Ignoring non-one normal map scale on material '{}'", name);
    }

    // TODO: Would it be worth submitting invidual texture load tasklets?
    String albedo_path = "/default_albedo";
    String normal_path = "/default_normal";
    if (std::uint64_t albedo_index; pbr_info["baseColorTexture"]["index"].get(albedo_index) == simdjson::SUCCESS) {
        process_texture(albedo_index, albedo_path, vull::format("/materials/{}/albedo", name), TextureType::Albedo);
    }
    if (std::uint64_t normal_index; normal_info["index"].get(normal_index) == simdjson::SUCCESS) {
        process_texture(normal_index, normal_path, vull::format("/materials/{}/normal", name), TextureType::Normal);
    }

    ScopedLock lock(m_material_map_mutex);
    m_albedo_paths.set(index, albedo_path);
    m_normal_paths.set(index, normal_path);
    return true;
}

// TODO: Missing validation in some places.
bool Converter::process_primitive(const simdjson::dom::object &primitive, String &&name) {
    uint64_t positions_index;
    EXPECT_SUCCESS(primitive["attributes"]["POSITION"].get(positions_index), "Missing vertex position attribute")
    uint64_t normals_index;
    EXPECT_SUCCESS(primitive["attributes"]["NORMAL"].get(normals_index), "Missing vertex normal attribute")
    uint64_t uvs_index;
    EXPECT_SUCCESS(primitive["attributes"]["TEXCOORD_0"].get(uvs_index), "Missing vertex texcoord attribute")
    uint64_t indices_index;
    EXPECT_SUCCESS(primitive["indices"].get(indices_index), "Missing indices")

    auto positions_accessor = m_document["accessors"].at(positions_index);
    auto normals_accessor = m_document["accessors"].at(normals_index);
    auto uvs_accessor = m_document["accessors"].at(uvs_index);
    auto indices_accessor = m_document["accessors"].at(indices_index);

    uint64_t vertex_count;
    EXPECT_SUCCESS(positions_accessor["count"].get(vertex_count), "Failed to get vertex count")
    uint64_t index_count;
    EXPECT_SUCCESS(indices_accessor["count"].get(index_count), "Failed to get index count")

    if (vertex_count > UINT32_MAX) {
        vull::error("[gltf] vertex_count > UINT32_MAX");
        return false;
    }
    if (index_count > UINT32_MAX) {
        vull::error("[gltf] index_count > UINT32_MAX");
        return false;
    }

    uint64_t positions_offset;
    EXPECT_SUCCESS(
        m_document["bufferViews"].at(positions_accessor["bufferView"].get_uint64().value_unsafe())["byteOffset"].get(
            positions_offset),
        "Failed to get vertex position data offset")
    uint64_t normals_offset;
    EXPECT_SUCCESS(
        m_document["bufferViews"].at(normals_accessor["bufferView"].get_uint64().value_unsafe())["byteOffset"].get(
            normals_offset),
        "Failed to get vertex normal data offset")
    uint64_t uvs_offset;
    EXPECT_SUCCESS(
        m_document["bufferViews"].at(uvs_accessor["bufferView"].get_uint64().value_unsafe())["byteOffset"].get(
            uvs_offset),
        "Failed to get vertex texcoord data offset")
    uint64_t indices_offset;
    EXPECT_SUCCESS(
        m_document["bufferViews"].at(indices_accessor["bufferView"].get_uint64().value_unsafe())["byteOffset"].get(
            indices_offset),
        "Failed to get index data offset")

    Vector<Vertex> vertices(static_cast<uint32_t>(vertex_count));
    for (auto &vertex : vertices) {
        memcpy(&vertex.position, &m_binary_blob[positions_offset], sizeof(Vec3f));
        memcpy(&vertex.normal, &m_binary_blob[normals_offset], sizeof(Vec3f));
        memcpy(&vertex.uv, &m_binary_blob[uvs_offset], sizeof(Vec2f));
        positions_offset += sizeof(Vec3f);
        normals_offset += sizeof(Vec3f);
        uvs_offset += sizeof(Vec2f);
    }

    uint64_t index_type;
    EXPECT_SUCCESS(indices_accessor["componentType"].get(index_type), "Failed to get index component type")

    size_t index_size;
    switch (index_type) {
    case 5121:
        index_size = sizeof(uint8_t);
        break;
    case 5123:
        index_size = sizeof(uint16_t);
        break;
    case 5125:
        index_size = sizeof(uint32_t);
        break;
    default:
        vull::error("[gltf] Unknown index type {}", index_type);
        return false;
    }

    Vector<uint32_t> indices(static_cast<uint32_t>(index_count));
    for (auto &index : indices) {
        const auto *index_bytes = &m_binary_blob[indices_offset];
        switch (index_size) {
        case sizeof(uint32_t):
            index |= static_cast<uint32_t>(index_bytes[3]) << 24u;
            index |= static_cast<uint32_t>(index_bytes[2]) << 16u;
            [[fallthrough]];
        case sizeof(uint16_t):
            index |= static_cast<uint32_t>(index_bytes[1]) << 8u;
            [[fallthrough]];
        case sizeof(uint8_t):
            index |= static_cast<uint32_t>(index_bytes[0]) << 0u;
            break;
        }
        indices_offset += index_size;
    }

    // TODO: Don't do this if --fast passed.
    meshopt_optimizeVertexCache(indices.data(), indices.data(), indices.size(), vertices.size());
    meshopt_optimizeVertexFetch(vertices.data(), indices.data(), indices.size(), vertices.data(), vertices.size(),
                                sizeof(Vertex));

    auto vertex_data_entry = m_pack_writer.start_entry(vull::format("/meshes/{}/vertex", name), vpak::EntryType::Blob);
    VULL_EXPECT(vertex_data_entry.write(vertices.span()));
    vertex_data_entry.finish();

    auto index_data_entry = m_pack_writer.start_entry(vull::format("/meshes/{}/index", name), vpak::EntryType::Blob);
    VULL_EXPECT(index_data_entry.write(indices.span()));
    index_data_entry.finish();

    Vec3f aabb_min(FLT_MAX);
    Vec3f aabb_max(FLT_MIN);
    Vec3f sphere_center;
    for (const auto &vertex : vertices) {
        aabb_min = vull::min(aabb_min, vertex.position);
        aabb_max = vull::max(aabb_max, vertex.position);
        sphere_center += vertex.position;
    }
    sphere_center /= static_cast<float>(vertices.size());

    float sphere_radius = 0;
    for (const auto &vertex : vertices) {
        sphere_radius = vull::max(sphere_radius, vull::distance(sphere_center, vertex.position));
    }

    BoundingBox bounding_box((aabb_min + aabb_max) * 0.5f, (aabb_max - aabb_min) * 0.5f);
    BoundingSphere bounding_sphere(sphere_center, sphere_radius);
    ScopedLock lock(m_mesh_bounds_mutex);
    m_mesh_bounds.set(vull::move(name), {bounding_box, bounding_sphere});
    return true;
}

bool array_to_vec(const simdjson::dom::array &array, auto &vec) {
    if (array.size() != vull::remove_ref<decltype(vec)>::length) {
        vull::error("Array wrong size in array_to_vec");
        return false;
    }
    for (size_t i = 0; i < array.size(); i++) {
        double elem;
        EXPECT_SUCCESS(array.at(i).get(elem), "Failed to index double array")
        vec[static_cast<unsigned>(i)] = static_cast<float>(elem);
    }
    return true;
}

Material Converter::make_material(simdjson::simdjson_result<simdjson::dom::element> primitive) {
    uint64_t index;
    if (primitive["material"].get(index) != simdjson::SUCCESS) {
        return {"/default_albedo", "/default_normal"};
    }

    String albedo_path = "/default_albedo";
    if (auto path = m_albedo_paths.get(index)) {
        albedo_path = String(*path);
    }

    String normal_path = "/default_normal";
    if (auto path = m_normal_paths.get(index)) {
        normal_path = String(*path);
    }
    return {vull::move(albedo_path), vull::move(normal_path)};
}

bool Converter::visit_node(World &world, uint64_t index, EntityId parent_id) {
    simdjson::dom::object node;
    EXPECT_SUCCESS(m_document["nodes"].at(index).get(node), "Failed to index node array")

    // TODO: Handle this.
    VULL_ENSURE(node["matrix"].error() == simdjson::error_code::NO_SUCH_FIELD);

    Vec3f position;
    if (simdjson::dom::array position_array; node["translation"].get(position_array) == simdjson::SUCCESS) {
        if (!array_to_vec(position_array, position)) {
            return false;
        }
    }

    Quatf rotation;
    if (simdjson::dom::array rotation_array; node["rotation"].get(rotation_array) == simdjson::SUCCESS) {
        if (!array_to_vec(rotation_array, rotation)) {
            return false;
        }
    }

    Vec3f scale(1.0f);
    if (simdjson::dom::array scale_array; node["scale"].get(scale_array) == simdjson::SUCCESS) {
        if (!array_to_vec(scale_array, scale)) {
            return false;
        }
    }

    auto entity = world.create_entity();
    entity.add<Transform>(parent_id, position, rotation, scale);
    if (uint64_t mesh_index; node["mesh"].get(mesh_index) == simdjson::SUCCESS) {
        std::string_view mesh_name_;
        EXPECT_SUCCESS(m_document["meshes"].at(mesh_index)["name"].get(mesh_name_), "Missing mesh name")
        StringView mesh_name(mesh_name_.data(), mesh_name_.length());

        simdjson::dom::array primitives;
        EXPECT_SUCCESS(m_document["meshes"].at(mesh_index)["primitives"].get(primitives),
                       "Failed to get primitives array")
        if (primitives.size() == 1) {
            entity.add<Mesh>(vull::format("/meshes/{}.0/vertex", mesh_name),
                             vull::format("/meshes/{}.0/index", mesh_name));
            entity.add<Material>(make_material(primitives.at(0)));
            if (auto bounds = m_mesh_bounds.get(vull::format("{}.0", mesh_name))) {
                entity.add<BoundingBox>(bounds->box);
                entity.add<BoundingSphere>(bounds->sphere);
            }
        } else {
            for (uint64_t i = 0; i < primitives.size(); i++) {
                auto sub_entity = world.create_entity();
                sub_entity.add<Transform>(entity);
                sub_entity.add<Mesh>(vull::format("/meshes/{}.{}/vertex", mesh_name, i),
                                     vull::format("/meshes/{}.{}/index", mesh_name, i));
                sub_entity.add<Material>(make_material(primitives.at(i)));
                if (auto bounds = m_mesh_bounds.get(vull::format("{}.{}", mesh_name, i))) {
                    sub_entity.add<BoundingBox>(bounds->box);
                    sub_entity.add<BoundingSphere>(bounds->sphere);
                }
            }
        }
    }

    if (simdjson::dom::array children; node["children"].get(children) == simdjson::SUCCESS) {
        for (auto child : children) {
            uint64_t child_index;
            EXPECT_SUCCESS(child.get(child_index), "Child node index not an integer")
            if (!visit_node(world, child_index, entity)) {
                return false;
            }
        }
    }
    return true;
}

} // namespace

FileMmap::FileMmap(int fd, size_t size) : m_size(size) {
    m_data = static_cast<uint8_t *>(mmap(nullptr, m_size, PROT_READ, MAP_PRIVATE, fd, 0));
    if (m_data == MAP_FAILED) {
        m_data = nullptr;
    }
}

FileMmap::~FileMmap() {
    if (m_data != nullptr) {
        munmap(m_data, m_size);
    }
}

FileMmap &FileMmap::operator=(FileMmap &&other) {
    m_data = vull::exchange(other.m_data, nullptr);
    m_size = vull::exchange(other.m_size, 0u);
    return *this;
}

bool GltfParser::parse_glb(StringView input_path) {
    FILE *file = fopen(input_path.data(), "rb");
    if (file == nullptr) {
        vull::error("[gltf] Failed to open {}", input_path);
        return false;
    }

    struct stat stat {};
    if (fstat(fileno(file), &stat) < 0) {
        vull::error("[gltf] Failed to stat {}", input_path);
        fclose(file);
        return false;
    }

    const auto file_size = static_cast<size_t>(stat.st_size);
    if (file_size < 28) {
        vull::error("[gltf] Less than minimum size of 28 bytes");
        fclose(file);
        return false;
    }

    m_data = {fileno(file), file_size};
    fclose(file);
    if (!m_data) {
        vull::error("[gltf] Failed to mmap");
        return false;
    }

    // Validate magic number.
    if (const auto magic = DWORD_LE(m_data, 0); magic != 0x46546c67u) {
        vull::error("[gltf] Invalid magic number {h}", magic);
        return false;
    }

    // Validate version.
    if (const auto version = DWORD_LE(m_data, 4); version != 2u) {
        vull::error("[gltf] Unsupported version {}", version);
        return false;
    }

    // Validate that the alleged size in the header is the actual size.
    if (const auto size = DWORD_LE(m_data, 8); size != file_size) {
        vull::error("[gltf] Size mismatch ({} vs {})", size, file_size);
        return false;
    }

    // glTF 2 must have a single JSON chunk at the start.
    const auto json_length = DWORD_LE(m_data, 12);
    if (DWORD_LE(m_data, 16) != 0x4e4f534au) {
        vull::error("[gltf] Missing or invalid JSON chunk");
        return false;
    }
    m_json = {reinterpret_cast<const char *>(&m_data[20]), json_length};

    // Followed by a binary chunk.
    if (DWORD_LE(m_data, 20 + json_length + 4) != 0x004e4942u) {
        vull::error("[gltf] Missing or invalid binary chunk");
        return false;
    }
    m_binary_blob = &m_data[20 + json_length + 8];
    return true;
}

bool GltfParser::convert(vpak::Writer &pack_writer, bool max_resolution, bool reproducible) {
    simdjson::dom::parser parser;
    simdjson::dom::element document;
    if (auto error = parser.parse(m_json.data(), m_json.length(), false).get(document)) {
        vull::error("[gltf] JSON parse error: {}", simdjson::error_message(error));
        return false;
    }

    if (simdjson::dom::array extensions; document["extensionsRequired"].get(extensions) == simdjson::SUCCESS) {
        for (auto extension : extensions) {
            std::string_view name;
            EXPECT_SUCCESS(extension.get(name), "Extension not a string")
            vull::warn("[gltf] Required extension {} not supported", StringView(name.data(), name.length()));
        }
    }

    simdjson::dom::object asset_info;
    if (auto error = document["asset"].get(asset_info)) {
        vull::error("[gltf] Failed to get asset info: {}", simdjson::error_message(error));
        return false;
    }
    if (std::string_view generator; asset_info["generator"].get(generator) == simdjson::SUCCESS) {
        vull::info("[gltf] Generator: {}", StringView(generator.data(), generator.length()));
    }

    Converter converter(m_binary_blob, pack_writer, document, max_resolution);
    Scheduler scheduler(reproducible ? 1 : 0);
    Latch latch;
    Atomic<bool> success = true;

    auto *tasklet = Tasklet::create();
    tasklet->set_callable([&] {
        success.store(converter.convert(latch));
    });
    scheduler.start(tasklet);
    latch.wait();
    scheduler.stop();

    if (!success.load()) {
        return false;
    }

    if (simdjson::dom::array scenes; document["scenes"].get(scenes) == simdjson::SUCCESS) {
        for (auto scene : scenes) {
            std::string_view name_;
            if (scene["name"].get(name_) != simdjson::SUCCESS) {
                vull::warn("[gltf] Ignoring scene with no name");
                continue;
            }
            StringView name(name_.data(), name_.length());
            vull::info("[gltf] Creating scene '{}'", name);

            World world;
            world.register_component<Transform>();
            world.register_component<Mesh>();
            world.register_component<Material>();
            world.register_component<BoundingBox>();
            world.register_component<BoundingSphere>();

            if (simdjson::dom::array root_nodes; scene["nodes"].get(root_nodes) == simdjson::SUCCESS) {
                for (auto node : root_nodes) {
                    uint64_t index;
                    EXPECT_SUCCESS(node.get(index), "Root node index not an integer")
                    if (!converter.visit_node(world, index, ~EntityId(0))) {
                        return false;
                    }
                }
            }

            const auto entry_name = vull::format("/scenes/{}", name);
            VULL_EXPECT(world.serialise(pack_writer, entry_name));
        }
    }
    return true;
}

} // namespace vull
