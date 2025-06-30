#include "gltf_parser.hh"

#include "float_image.hh"
#include "mad_inst.hh"
#include "png_stream.hh"

#include <vull/container/hash_map.hh>
#include <vull/container/perfect_map.hh>
#include <vull/container/vector.hh>
#include <vull/core/bounding_box.hh>
#include <vull/core/bounding_sphere.hh>
#include <vull/core/log.hh>
#include <vull/ecs/world.hh>
#include <vull/graphics/material.hh>
#include <vull/graphics/mesh.hh>
#include <vull/graphics/vertex.hh>
#include <vull/json/parser.hh>
#include <vull/json/tree.hh>
#include <vull/maths/vec.hh>
#include <vull/platform/timer.hh>
#include <vull/scene/transform.hh>
#include <vull/support/assert.hh>
#include <vull/support/atomic.hh>
#include <vull/support/scoped_lock.hh>
#include <vull/support/span_stream.hh>
#include <vull/support/string_builder.hh>
#include <vull/tasklet/functions.hh>
#include <vull/tasklet/future.hh>
#include <vull/tasklet/latch.hh>
#include <vull/tasklet/mutex.hh>
#include <vull/tasklet/scheduler.hh>
#include <vull/tasklet/tasklet.hh>
#include <vull/vpak/pack_file.hh>
#include <vull/vpak/stream.hh>
#include <vull/vpak/writer.hh>

#include <float.h>
#include <meshoptimizer.h>

// TODO(json): Lots of casting to uint64_t from a get<int64_t>().
// TODO(json): Range-based for loops on arrays.

namespace vull {
namespace {

enum class TextureType {
    Albedo,
    Normal,
};

class Converter {
    Span<uint8_t> m_binary_blob;
    vpak::Writer &m_pack_writer;
    json::Value &m_document;
    const bool m_max_resolution;

    HashMap<uint64_t, String> m_albedo_paths;
    HashMap<uint64_t, String> m_normal_paths;
    tasklet::Mutex m_material_map_mutex;

    struct MeshBounds {
        BoundingBox box;
        BoundingSphere sphere;
    };
    HashMap<String, MeshBounds> m_mesh_bounds;
    tasklet::Mutex m_mesh_bounds_mutex;

public:
    Converter(Span<uint8_t> binary_blob, vpak::Writer &pack_writer, json::Value &document, bool max_resolution)
        : m_binary_blob(binary_blob), m_pack_writer(pack_writer), m_document(document),
          m_max_resolution(max_resolution) {}

    GltfResult<Tuple<uint64_t, uint64_t>> get_blob_info(const json::Object &accessor);
    GltfResult<uint64_t> get_blob_offset(const json::Object &accessor);

    GltfResult<> process_texture(TextureType type, const String &path, Optional<uint64_t> index,
                                 Optional<Vec4f> colour_factor);
    GltfResult<> process_material(const json::Object &material, uint64_t index);

    GltfResult<> process_primitive(const json::Object &primitive, String &&name);

    GltfResult<Optional<Material>> make_material(const json::Object &primitive);
    GltfResult<> visit_node(World &world, EntityId parent_id, uint64_t index);
    GltfResult<> process_scene(const json::Object &scene, StringView name);

    GltfResult<> convert();
};

GltfResult<Tuple<uint64_t, uint64_t>> Converter::get_blob_info(const json::Object &accessor) {
    uint64_t accessor_offset = 0;
    if (accessor["byteOffset"].has<int64_t>()) {
        accessor_offset = static_cast<uint64_t>(VULL_ASSUME(accessor["byteOffset"].get<int64_t>()));
    }

    const auto buffer_view_index = static_cast<uint64_t>(VULL_TRY(accessor["bufferView"].get<int64_t>()));
    const auto &buffer_view = VULL_TRY(m_document["bufferViews"][buffer_view_index].get<json::Object>());
    const auto view_length = static_cast<uint64_t>(VULL_TRY(buffer_view["byteLength"].get<int64_t>()));

    uint64_t view_offset = 0;
    if (buffer_view["byteOffset"].has<int64_t>()) {
        view_offset = static_cast<uint64_t>(VULL_ASSUME(buffer_view["byteOffset"].get<int64_t>()));
    }

    // TODO: Assuming buffer == 0 here; support external blobs.
    const auto combined_offset = accessor_offset + view_offset;
    if (combined_offset + view_length > m_binary_blob.size()) {
        return GltfError::OffsetOutOfBounds;
    }
    return vull::make_tuple(combined_offset, static_cast<uint64_t>(view_length));
}

GltfResult<uint64_t> Converter::get_blob_offset(const json::Object &accessor) {
    return vull::get<0>(VULL_TRY(get_blob_info(accessor)));
}

GltfResult<> validate_accessor(const json::Object &accessor) {
    // TODO: Should probably check componentType and type as well.
    if (accessor["normalized"].has<bool>() && VULL_ASSUME(accessor["normalized"].get<bool>())) {
        return GltfError::UnsupportedNormalisedAccessor;
    }
    if (accessor["sparse"].has<bool>() && VULL_ASSUME(accessor["sparse"].get<bool>())) {
        return GltfError::UnsupportedSparseAccessor;
    }
    return {};
}

vpak::ImageFilter convert_filter(int64_t type) {
    switch (type) {
    case 9729:
        return vpak::ImageFilter::Linear;
    case 9984:
        return vpak::ImageFilter::NearestMipmapNearest;
    case 9985:
        return vpak::ImageFilter::LinearMipmapNearest;
    case 9986:
        return vpak::ImageFilter::NearestMipmapLinear;
    case 9987:
        return vpak::ImageFilter::LinearMipmapLinear;
    default:
        return vpak::ImageFilter::Nearest;
    }
}

vpak::ImageWrapMode convert_wrap_mode(int64_t type) {
    switch (type) {
    case 33071:
        return vpak::ImageWrapMode::ClampToEdge;
    case 33648:
        return vpak::ImageWrapMode::MirroredRepeat;
    default:
        return vpak::ImageWrapMode::Repeat;
    }
}

GltfResult<> Converter::process_texture(TextureType type, const String &path, Optional<uint64_t> index,
                                        Optional<Vec4f> colour_factor) {
    Filter mipmap_filter;
    vpak::ImageFormat vpak_format;
    switch (type) {
    case TextureType::Albedo:
        mipmap_filter = Filter::Box;
        vpak_format = vpak::ImageFormat::Bc7Srgb;
        break;
    case TextureType::Normal:
        mipmap_filter = Filter::Gaussian;
        vpak_format = vpak::ImageFormat::Bc5Unorm;
        break;
    }

    // When undefined, a sampler with repeat wrapping and auto filtering SHOULD be used.
    auto mag_filter = vpak::ImageFilter::Linear;
    auto min_filter = vpak::ImageFilter::Linear;
    auto wrap_u = vpak::ImageWrapMode::Repeat;
    auto wrap_v = vpak::ImageWrapMode::Repeat;

    FloatImage float_image;
    if (index) {
        const auto &texture = VULL_TRY(m_document["textures"][*index]);
        const auto image_index = VULL_TRY(texture["source"].get<int64_t>());
        const auto &image = VULL_TRY(m_document["images"][image_index].get<json::Object>());

        const auto &mime_type = VULL_TRY(image["mimeType"].get<String>());
        if (mime_type != "image/png") {
            // TODO: Don't error, just fallback to error texture?
            return GltfError::UnsupportedImageMimeType;
        }

        auto blob_info = VULL_TRY(get_blob_info(image));
        auto span_stream =
            vull::make_unique<SpanStream>(m_binary_blob.subspan(vull::get<0>(blob_info), vull::get<1>(blob_info)));
        auto png_stream = VULL_TRY(PngStream::create(vull::move(span_stream)));

        auto unorm_data = ByteBuffer::create_uninitialised(png_stream.row_byte_count() * png_stream.height());
        for (uint32_t y = 0; y < png_stream.height(); y++) {
            png_stream.read_row(unorm_data.span().subspan(y * png_stream.row_byte_count()));
        }

        float_image = FloatImage::from_unorm(unorm_data.span(), Vec2u(png_stream.width(), png_stream.height()),
                                             png_stream.pixel_byte_count());

        if (texture["sampler"]) {
            const auto sampler_index = VULL_TRY(texture["sampler"].get<int64_t>());
            const auto &sampler = VULL_TRY(m_document["samplers"][sampler_index].get<json::Object>());
            if (sampler["magFilter"]) {
                mag_filter = convert_filter(VULL_TRY(sampler["magFilter"].get<int64_t>()));
            }
            if (sampler["minFilter"]) {
                min_filter = convert_filter(VULL_TRY(sampler["minFilter"].get<int64_t>()));
            }
            if (sampler["wrapS"]) {
                wrap_u = convert_wrap_mode(VULL_TRY(sampler["wrapS"].get<int64_t>()));
            }
            if (sampler["wrapT"]) {
                wrap_v = convert_wrap_mode(VULL_TRY(sampler["wrapT"].get<int64_t>()));
            }
        }
    }

    if (colour_factor && float_image.mip_count() != 0) {
        // Explicit colour factor to multiply with image.
        // TODO: Multiply with image.
        VULL_ENSURE_NOT_REACHED();
    } else if (colour_factor) {
        // Explicit colour factor but no image, solid colour.
        float_image = FloatImage::from_colour(Colour::from_rgb(*colour_factor));
    } else if (float_image.mip_count() == 0) {
        // No image or explicit colour factor, assume white.
        float_image = FloatImage::from_colour(Colour::white());
    }

    if (type == TextureType::Normal) {
        float_image.colours_to_vectors();
    }
    float_image.build_mipchain(mipmap_filter);
    if (type == TextureType::Normal) {
        float_image.normalise();
        float_image.vectors_to_colours();
    }

    // Drop first mip if not wanting max resolution textures.
    constexpr uint32_t log_threshold_resolution = 11u;
    if (!m_max_resolution && float_image.mip_count() > log_threshold_resolution) {
        float_image.drop_mips(1);
    }

    auto stream = m_pack_writer.add_entry(path, vpak::EntryType::Image);
    VULL_TRY(stream.write_byte(vull::to_underlying(vpak_format)));
    VULL_TRY(stream.write_byte(vull::to_underlying(mag_filter)));
    VULL_TRY(stream.write_byte(vull::to_underlying(min_filter)));
    VULL_TRY(stream.write_byte(vull::to_underlying(wrap_u)));
    VULL_TRY(stream.write_byte(vull::to_underlying(wrap_v)));
    VULL_TRY(stream.write_varint(float_image.size().x()));
    VULL_TRY(stream.write_varint(float_image.size().y()));
    VULL_TRY(stream.write_varint(float_image.mip_count()));
    VULL_TRY(float_image.block_compress(stream, vpak_format == vpak::ImageFormat::Bc5Unorm));
    VULL_TRY(stream.finish());
    return {};
}

GltfResult<> Converter::process_material(const json::Object &material, uint64_t index) {
    String name;
    if (material["name"].has<String>()) {
        name = String(VULL_ASSUME(material["name"].get<String>()));
    } else {
        name = vull::format("material{}", index);
    }

    if (material["occlusionTexture"]) {
        vull::warn("[gltf] Ignoring unsupported occlusion texture on material '{}'", name);
    }
    if (material["emissiveTexture"] || material["emissiveFactor"]) {
        vull::warn("[gltf] Ignoring unsupported emissive properties on material '{}'", name);
    }

    // TODO(json): .get_or(fallback_value) function.
    StringView alpha_mode = "OPAQUE";
    double alpha_cutoff = 0.5;
    bool double_sided = false;
    if (material["alphaMode"].has<String>()) {
        alpha_mode = VULL_ASSUME(material["alphaMode"].get<String>());
    }
    if (material["alphaCutoff"].has<double>()) {
        alpha_cutoff = VULL_ASSUME(material["alphaCutoff"].get<double>());
    }
    if (material["doubleSided"].has<bool>()) {
        double_sided = VULL_ASSUME(material["doubleSided"].get<bool>());
    }

    if (alpha_mode != "OPAQUE") {
        vull::warn("[gltf] Ignoring unsupported alpha mode {} on material '{}'", alpha_mode, name);
    }
    if (alpha_cutoff != 0.5) {
        vull::warn("[gltf] Ignoring non-default alpha cutoff of {} on material '{}'", alpha_cutoff, name);
    }
    if (double_sided) {
        vull::warn("[gltf] Ignoring unsupported double sided property on material '{}'", name);
    }

    // TODO: In addition to the material properties, if a primitive specifies a vertex color using the attribute
    //       semantic property COLOR_0, then this value acts as an additional linear multiplier to base color.
    auto pbr_info = material["pbrMetallicRoughness"];
    if (pbr_info) {
        Optional<uint64_t> texture_index;
        if (pbr_info["baseColorTexture"]["index"].has<int64_t>()) {
            texture_index = static_cast<uint64_t>(VULL_ASSUME(pbr_info["baseColorTexture"]["index"].get<int64_t>()));
        }
        Optional<Vec4f> colour_factor;
        if (pbr_info["baseColorFactor"].has<json::Array>()) {
            const auto &factor = VULL_ASSUME(pbr_info["baseColorFactor"].get<json::Array>());
            colour_factor = Vec4f(VULL_TRY(factor[0].get<double>()), VULL_TRY(factor[1].get<double>()),
                                  VULL_TRY(factor[2].get<double>()), VULL_TRY(factor[3].get<double>()));
        }

        const auto path = vull::format("/materials/{}/albedo", name);
        VULL_TRY(process_texture(TextureType::Albedo, path, texture_index, colour_factor));

        ScopedLock lock(m_material_map_mutex);
        m_albedo_paths.set(index, path);
    }

    const auto normal_info = material["normalTexture"];
    if (normal_info) {
        int64_t tex_coord = 0;
        double scale = 1.0;
        if (normal_info["texCoord"].has<int64_t>()) {
            tex_coord = VULL_ASSUME(normal_info["texCoord"].get<int64_t>());
        }
        if (normal_info["scale"].has<double>()) {
            scale = VULL_ASSUME(normal_info["scale"].get<double>());
        }

        if (tex_coord != 0) {
            vull::warn("[gltf] Ignoring unsupported texCoord attribute index {} on material '{}'", tex_coord, name);
        }
        if (scale != 1.0) {
            vull::warn("[gltf] Ignoring non-one normal map scale of {} on material '{}'", scale, name);
        }

        const auto path = vull::format("/materials/{}/normal", name);
        const auto texture_index = static_cast<uint64_t>(VULL_TRY(normal_info["index"].get<int64_t>()));
        VULL_TRY(process_texture(TextureType::Normal, path, texture_index, {}));

        ScopedLock lock(m_material_map_mutex);
        m_normal_paths.set(index, path);
    }
    return {};
}

size_t convert_index_type(int64_t index_type) {
    // This produces better code than a switch.
    const auto mapped = static_cast<size_t>(index_type) - 5121;
    return mapped != 0 ? mapped : 1;
}

GltfResult<> Converter::process_primitive(const json::Object &primitive, String &&name) {
    if (primitive["mode"].has<int64_t>()) {
        const auto mode = VULL_ASSUME(primitive["mode"].get<int64_t>());
        if (mode != 4) {
            // Not triangles.
            return GltfError::UnsupportedPrimitiveMode;
        }
    }

    // Get accessors for all the attributes we care about.
    // TODO: Spec says we should support two UV sets, vertex colours, etc.
    const auto position_accessor_index = VULL_TRY(primitive["attributes"]["POSITION"].get<int64_t>());
    const auto normal_accessor_index = VULL_TRY(primitive["attributes"]["NORMAL"].get<int64_t>());
    const auto uv_accessor_index = VULL_TRY(primitive["attributes"]["TEXCOORD_0"].get<int64_t>());
    const auto index_accessor_index = VULL_TRY(primitive["indices"].get<int64_t>());
    const auto &position_accessor = VULL_TRY(m_document["accessors"][position_accessor_index].get<json::Object>());
    const auto &normal_accessor = VULL_TRY(m_document["accessors"][normal_accessor_index].get<json::Object>());
    const auto &uv_accessor = VULL_TRY(m_document["accessors"][uv_accessor_index].get<json::Object>());
    const auto &index_accessor = VULL_TRY(m_document["accessors"][index_accessor_index].get<json::Object>());

    // Validate the accessors.
    VULL_TRY(validate_accessor(position_accessor));
    VULL_TRY(validate_accessor(normal_accessor));
    VULL_TRY(validate_accessor(uv_accessor));
    VULL_TRY(validate_accessor(index_accessor));

    const auto index_count = static_cast<uint64_t>(VULL_TRY(index_accessor["count"].get<int64_t>()));
    const auto index_size = convert_index_type(VULL_TRY(index_accessor["componentType"].get<int64_t>()));
    auto index_offset = VULL_TRY(get_blob_offset(index_accessor));

    auto indices = FixedBuffer<uint32_t>::create_zeroed(index_count);
    for (auto &index : indices) {
        auto index_bytes = m_binary_blob.subspan(index_offset, index_size);
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
        default:
            vull::unreachable();
        }
        index_offset += index_size;
    }

    const auto vertex_count = static_cast<uint64_t>(VULL_TRY(position_accessor["count"].get<int64_t>()));
    auto position_offset = VULL_TRY(get_blob_offset(position_accessor));
    auto normal_offset = VULL_TRY(get_blob_offset(normal_accessor));
    auto uv_offset = VULL_TRY(get_blob_offset(uv_accessor));

    Vec3f aabb_min(FLT_MAX);
    Vec3f aabb_max(FLT_MIN);
    Vec3f sphere_center;
    auto vertices = FixedBuffer<Vertex>::create_zeroed(vertex_count);
    for (auto &vertex : vertices) {
        Vec3f position;
        Vec3f normal;
        Vec2f uv;
        memcpy(&position, m_binary_blob.byte_offset(position_offset), sizeof(Vec3f));
        memcpy(&normal, m_binary_blob.byte_offset(normal_offset), sizeof(Vec3f));
        memcpy(&uv, m_binary_blob.byte_offset(uv_offset), sizeof(Vec2f));

        // Update bounding information.
        aabb_min = vull::min(aabb_min, position);
        aabb_max = vull::max(aabb_max, position);
        sphere_center += position;

        vertex.px = meshopt_quantizeHalf(position.x());
        vertex.py = meshopt_quantizeHalf(position.y());
        vertex.pz = meshopt_quantizeHalf(position.z());
        vertex.normal |= vull::quantize_snorm<10>(normal.x()) << 20u;
        vertex.normal |= vull::quantize_snorm<10>(normal.y()) << 10u;
        vertex.normal |= vull::quantize_snorm<10>(normal.z());
        vertex.uv |= static_cast<uint32_t>(meshopt_quantizeHalf(uv.x()));
        vertex.uv |= static_cast<uint32_t>(meshopt_quantizeHalf(uv.y())) << 16u;

        position_offset += sizeof(Vec3f);
        normal_offset += sizeof(Vec3f);
        uv_offset += sizeof(Vec2f);
    }

    sphere_center /= static_cast<float>(vertices.size());

    // Calculate sphere radius.
    // TODO: Avoid second pass over vertices?
    float sphere_radius = 0;
    position_offset = VULL_TRY(get_blob_offset(position_accessor));
    for (uint64_t i = 0; i < vertex_count; i++) {
        Vec3f position;
        memcpy(&position, m_binary_blob.byte_offset(position_offset), sizeof(Vec3f));
        sphere_radius = vull::max(sphere_radius, vull::distance(sphere_center, position));
        position_offset += sizeof(Vec3f);
    }

    // TODO: Don't do this if --fast passed.
    meshopt_optimizeVertexCache(indices.data(), indices.data(), indices.size(), vertices.size());
    meshopt_optimizeVertexFetch(vertices.data(), indices.data(), indices.size(), vertices.data(), vertices.size(),
                                sizeof(Vertex));

    auto vertex_data_entry = m_pack_writer.add_entry(vull::format("/meshes/{}/vertex", name), vpak::EntryType::Blob);
    VULL_TRY(vertex_data_entry.write(vertices.span()));
    VULL_TRY(vertex_data_entry.finish());

    auto index_data_entry = m_pack_writer.add_entry(vull::format("/meshes/{}/index", name), vpak::EntryType::Blob);
    VULL_TRY(index_data_entry.write(indices.span()));
    VULL_TRY(index_data_entry.finish());

    BoundingBox bounding_box((aabb_min + aabb_max) * 0.5f, (aabb_max - aabb_min) * 0.5f);
    BoundingSphere bounding_sphere(sphere_center, sphere_radius);
    ScopedLock lock(m_mesh_bounds_mutex);
    m_mesh_bounds.set(vull::move(name), {bounding_box, bounding_sphere});
    return {};
}

GltfResult<> array_to_vec(const json::Array &array, auto &vec) {
    if (array.size() != vull::remove_ref<decltype(vec)>::length) {
        return GltfError::BadVectorArrayLength;
    }
    for (uint64_t i = 0; i < array.size(); i++) {
        auto value = VULL_TRY(array[i].get<double>());
        vec[static_cast<unsigned>(i)] = static_cast<float>(value);
    }
    return {};
}

GltfResult<Optional<Material>> Converter::make_material(const json::Object &primitive) {
    if (!primitive["material"]) {
        return Optional<Material>();
    }

    auto index = static_cast<uint64_t>(VULL_TRY(primitive["material"].get<int64_t>()));
    auto albedo_path = m_albedo_paths.get(index).value_or({});
    auto normal_path = m_normal_paths.get(index).value_or({});
    return Optional<Material>(Material(vull::move(albedo_path), vull::move(normal_path)));
}

GltfResult<> Converter::visit_node(World &world, EntityId parent_id, uint64_t index) {
    const auto &node = VULL_TRY(m_document["nodes"][index].get<json::Object>());
    if (node["matrix"]) {
        return GltfError::UnsupportedNodeMatrix;
    }

    Vec3f position;
    if (auto translation_array = node["translation"].get<json::Array>().to_optional()) {
        VULL_TRY(array_to_vec(*translation_array, position));
    }

    Quatf rotation;
    if (auto rotation_array = node["rotation"].get<json::Array>().to_optional()) {
        VULL_TRY(array_to_vec(*rotation_array, rotation));
    }

    Vec3f scale(1.0f);
    if (auto scale_array = node["scale"].get<json::Array>().to_optional()) {
        VULL_TRY(array_to_vec(*scale_array, scale));
    }

    auto entity = world.create_entity();
    entity.add<Transform>(parent_id, position, rotation, scale);

    // TODO: Make this much simpler by not using name strings as keys?
    if (auto mesh_index = node["mesh"].get<int64_t>().to_optional()) {
        const auto &mesh = VULL_TRY(m_document["meshes"][*mesh_index].get<json::Object>());
        const auto &mesh_name = VULL_TRY(mesh["name"].get<String>());
        const auto &primitive_array = VULL_TRY(mesh["primitives"].get<json::Array>());

        auto build_entity = [&](Entity &sub_entity, uint64_t primitive_index) -> GltfResult<> {
            const auto &primitive = VULL_TRY(primitive_array[primitive_index].get<json::Object>());
            if (auto material = VULL_TRY(make_material(primitive))) {
                sub_entity.add<Material>(*material);
            }
            sub_entity.add<Mesh>(vull::format("/meshes/{}.{}/vertex", mesh_name, primitive_index),
                                 vull::format("/meshes/{}.{}/index", mesh_name, primitive_index));
            if (auto bounds = m_mesh_bounds.get(vull::format("{}.{}", mesh_name, primitive_index))) {
                sub_entity.add<BoundingBox>(bounds->box);
                sub_entity.add<BoundingSphere>(bounds->sphere);
            }
            return {};
        };

        if (primitive_array.size() == 1) {
            VULL_TRY(build_entity(entity, 0));
        } else {
            for (uint64_t i = 0; i < primitive_array.size(); i++) {
                auto sub_entity = world.create_entity();
                sub_entity.add<Transform>(entity);
                VULL_TRY(build_entity(sub_entity, i));
            }
        }
    }

    if (auto children_array = node["children"].get<json::Array>().to_optional()) {
        for (uint64_t i = 0; i < children_array->size(); i++) {
            auto child_index = static_cast<uint64_t>(VULL_TRY((*children_array)[i].get<int64_t>()));
            VULL_TRY(visit_node(world, entity, child_index));
        }
    }
    return {};
}

GltfResult<> Converter::process_scene(const json::Object &scene, StringView name) {
    World world;
    world.register_component<Transform>();
    world.register_component<Mesh>();
    world.register_component<Material>();
    world.register_component<BoundingBox>();
    world.register_component<BoundingSphere>();

    const auto &root_node_array = VULL_TRY(scene["nodes"].get<json::Array>());
    for (uint64_t i = 0; i < root_node_array.size(); i++) {
        const auto index = static_cast<uint64_t>(VULL_TRY(root_node_array[i].get<int64_t>()));
        VULL_TRY(visit_node(world, ~EntityId(0), index));
    }

    // Serialise to vpak.
    const auto entry_name = vull::format("/scenes/{}", name);
    VULL_TRY(world.serialise(m_pack_writer, entry_name));
    return {};
}

GltfResult<> Converter::convert() {
    Vector<tasklet::Future<GltfResult<>>> futures;

    const auto &material_array = VULL_TRY(m_document["materials"].get<json::Array>());
    futures.ensure_capacity(material_array.size());
    for (uint32_t index = 0; index < material_array.size(); index++) {
        const auto &material = VULL_TRY(material_array[index].get<json::Object>());
        futures.push(tasklet::schedule([this, &material, index]() -> GltfResult<> {
            VULL_TRY(process_material(material, index));
            return {};
        }));
    }

    const auto &mesh_array = VULL_TRY(m_document["meshes"].get<json::Array>());
    for (uint32_t mesh_index = 0; mesh_index < mesh_array.size(); mesh_index++) {
        // TODO: Spec doesn't require meshes to have a name (do what process_material does).
        const auto &mesh = VULL_TRY(mesh_array[mesh_index].get<json::Object>());
        const auto &mesh_name = VULL_TRY(mesh["name"].get<String>());
        const auto &primitive_array = VULL_TRY(mesh["primitives"].get<json::Array>());
        for (uint32_t primitive_index = 0; primitive_index < primitive_array.size(); primitive_index++) {
            const auto &primitive = VULL_TRY(primitive_array[primitive_index].get<json::Object>());
            auto name = vull::format("{}.{}", mesh_name, primitive_index);
            futures.push(tasklet::schedule([this, &primitive, name = vull::move(name)]() mutable -> GltfResult<> {
                VULL_TRY(process_primitive(primitive, vull::move(name)));
                return {};
            }));
        }
    }

    for (auto &future : futures) {
        VULL_TRY(future.await());
    }
    futures.clear();

    if (!m_document["scenes"]) {
        return {};
    }

    const auto &scene_array = VULL_TRY(m_document["scenes"].get<json::Array>());
    for (uint32_t scene_index = 0; scene_index < scene_array.size(); scene_index++) {
        const auto &scene = VULL_TRY(scene_array[scene_index].get<json::Object>());
        const auto &name = VULL_TRY(scene["name"].get<String>());
        vull::info("[gltf] Creating scene '{}'", name);
        futures.push(tasklet::schedule([this, &scene, &name]() -> GltfResult<> {
            VULL_TRY(process_scene(scene, name));
            return {};
        }));
    }

    for (auto &future : futures) {
        VULL_TRY(future.await());
    }
    return {};
}

} // namespace

Result<void, GlbError, StreamError> GltfParser::parse_glb() {
    if (const auto magic = VULL_TRY(m_stream.read_le<uint32_t>()); magic != 0x46546c67u) {
        vull::error("[gltf] Invalid magic number: {h}", magic);
        return GlbError::InvalidMagic;
    }

    if (const auto version = VULL_TRY(m_stream.read_le<uint32_t>()); version != 2u) {
        vull::error("[gltf] Unsupported version: {}", version);
        return GlbError::UnsupportedVersion;
    }

    // Ignore header size.
    VULL_TRY(m_stream.read_le<uint32_t>());

    // glTF 2 must have a single JSON chunk at the start.
    const auto json_length = VULL_TRY(m_stream.read_le<uint32_t>());
    if (VULL_TRY(m_stream.read_le<uint32_t>()) != 0x4e4f534au) {
        vull::error("[gltf] Missing or invalid JSON chunk");
        return GlbError::BadJsonChunk;
    }
    m_json = String(json_length);
    VULL_TRY(m_stream.read({m_json.data(), json_length}));

    // Followed by a binary chunk.
    const auto binary_length = VULL_TRY(m_stream.read_le<uint32_t>());
    if (VULL_TRY(m_stream.read_le<uint32_t>()) != 0x004e4942u) {
        vull::error("[gltf] Missing or invalid binary chunk");
        return GlbError::BadBinaryChunk;
    }
    m_binary_blob = ByteBuffer::create_uninitialised(binary_length);
    VULL_TRY(m_stream.read(m_binary_blob.span()));
    return {};
}

GltfResult<> GltfParser::convert(vpak::Writer &pack_writer, bool max_resolution, bool reproducible) {
    auto document = VULL_TRY(json::parse(m_json));
    if (auto generator = document["asset"]["generator"].get<String>()) {
        vull::info("[gltf] Generator: {}", generator.value());
    }

    if (auto required_extensions = document["extensionsRequired"].get<json::Array>().to_optional()) {
        for (uint32_t i = 0; i < required_extensions->size(); i++) {
            auto name = VULL_TRY((*required_extensions)[i].get<String>());
            vull::warn("[gltf] Required extension {} not supported", name);
        }
    }

    // Use only one thread if reproducible, otherwise let scheduler decide.
    tasklet::Scheduler scheduler(reproducible ? 1 : 0);
    VULL_TRY(scheduler.run([&]() -> GltfResult<> {
        Converter converter(m_binary_blob.span(), pack_writer, document, max_resolution);
        VULL_TRY(converter.convert());
        return {};
    }));
    return {};
}

} // namespace vull
