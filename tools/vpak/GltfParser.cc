#include "GltfParser.hh"

#include <vull/core/Log.hh>
#include <vull/core/Material.hh>
#include <vull/core/Mesh.hh>
#include <vull/core/Transform.hh>
#include <vull/core/Vertex.hh>
#include <vull/ecs/World.hh>
#include <vull/maths/Vec.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Atomic.hh>
#include <vull/support/Format.hh>
#include <vull/support/PerfectMap.hh>
#include <vull/support/Timer.hh>
#include <vull/support/Vector.hh>
#include <vull/tasklet/Scheduler.hh>
#include <vull/tasklet/Tasklet.hh>
#include <vull/vpak/PackFile.hh>
#include <vull/vpak/Writer.hh>

#include <linux/futex.h>
#include <meshoptimizer.h>
#include <simdjson.h>
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

namespace {

class Converter {
    const uint8_t *const m_binary_blob;
    vull::vpak::Writer &m_pack_writer;
    simdjson::dom::element &m_document;
    vull::World &m_world;

public:
    Converter(const uint8_t *binary_blob, vull::vpak::Writer &pack_writer, simdjson::dom::element &document,
              vull::World &world)
        : m_binary_blob(binary_blob), m_pack_writer(pack_writer), m_document(document), m_world(world) {}

    bool convert(vull::Atomic<uint32_t> &latch);
    bool process_primitive(const simdjson::dom::object &primitive, vull::StringView name);
    bool visit_node(uint64_t index, vull::EntityId parent_id);
};

bool Converter::convert(vull::Atomic<uint32_t> &latch) {
    // Emit default albedo texture.
    auto albedo_entry = m_pack_writer.start_entry("/default_albedo", vull::vpak::EntryType::ImageData);
    albedo_entry.write_byte(uint8_t(vull::vpak::ImageFormat::RgbaUnorm));
    albedo_entry.write_varint(16);
    albedo_entry.write_varint(16);
    albedo_entry.write_varint(1);
    constexpr vull::Array colours{
        vull::Vec<uint8_t, 4>(0xff, 0x69, 0xb4, 0xff),
        vull::Vec<uint8_t, 4>(0x94, 0x00, 0xd3, 0xff),
    };
    for (uint32_t y = 0; y < 16; y++) {
        for (uint32_t x = 0; x < 16; x++) {
            uint32_t colour_index = (x + y) % colours.size();
            albedo_entry.write({&colours[colour_index], 4});
        }
    }
    albedo_entry.finish();

    // Emit default normal map texture.
    auto normal_entry = m_pack_writer.start_entry("/default_normal", vull::vpak::EntryType::ImageData);
    normal_entry.write_byte(uint8_t(vull::vpak::ImageFormat::RgUnorm));
    normal_entry.write(vull::Array<uint8_t, 5>{1u, 1u, 1u, 127u, 127u}.span());
    normal_entry.finish();

    // Process meshes.
    simdjson::dom::array meshes;
    EXPECT_SUCCESS(m_document["meshes"].get(meshes), "Missing \"meshes\" property")
    for (auto mesh : meshes) {
        simdjson::dom::array primitives;
        EXPECT_SUCCESS(mesh["primitives"].get(primitives), "Missing \"primitives\" property")
        latch.fetch_add(static_cast<uint32_t>(primitives.size()), vull::MemoryOrder::Relaxed);
    }
    latch.fetch_sub(1, vull::MemoryOrder::Relaxed);

    for (auto mesh : meshes) {
        std::string_view mesh_name_;
        EXPECT_SUCCESS(mesh["name"].get(mesh_name_), "Missing mesh name")
        vull::StringView mesh_name(mesh_name_.data(), mesh_name_.length());

        simdjson::dom::array primitives;
        VULL_IGNORE(mesh["primitives"].get(primitives));
        for (uint64_t i = 0; i < primitives.size(); i++) {
            simdjson::dom::object primitive;
            EXPECT_SUCCESS(primitives.at(i).get(primitive), "Element in \"primitives\" array is not an object")
            vull::schedule([this, primitive, &latch, name = vull::format("{}.{}", mesh_name, i)] {
                process_primitive(primitive, name);
                if (latch.fetch_sub(1, vull::MemoryOrder::Relaxed) == 1) {
                    syscall(SYS_futex, latch.raw_ptr(), FUTEX_WAKE_PRIVATE, 1, nullptr, nullptr, 0);
                }
            });
        }
    }
    return true;
}

// TODO: Missing validation in some places.
bool Converter::process_primitive(const simdjson::dom::object &primitive, vull::StringView name) {
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

    vull::Vector<vull::Vertex> vertices(static_cast<uint32_t>(vertex_count));
    for (auto &vertex : vertices) {
        memcpy(&vertex.position, &m_binary_blob[positions_offset], sizeof(vull::Vec3f));
        memcpy(&vertex.normal, &m_binary_blob[normals_offset], sizeof(vull::Vec3f));
        memcpy(&vertex.uv, &m_binary_blob[uvs_offset], sizeof(vull::Vec2f));
        positions_offset += sizeof(vull::Vec3f);
        normals_offset += sizeof(vull::Vec3f);
        uvs_offset += sizeof(vull::Vec2f);
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

    vull::Vector<uint32_t> indices(static_cast<uint32_t>(index_count));
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
                                sizeof(vull::Vertex));

    auto vertex_data_entry =
        m_pack_writer.start_entry(vull::format("/meshes/{}/vertex", name), vull::vpak::EntryType::VertexData);
    vertex_data_entry.write(vertices.span());
    vertex_data_entry.finish();

    auto index_data_entry =
        m_pack_writer.start_entry(vull::format("/meshes/{}/index", name), vull::vpak::EntryType::IndexData);
    index_data_entry.write(indices.span());
    index_data_entry.finish();
    return true;
}

bool array_to_vec(const simdjson::dom::array &array, auto &vec) {
    if (array.size() != vull::RemoveRef<decltype(vec)>::length) {
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

bool Converter::visit_node(uint64_t index, vull::EntityId parent_id) {
    simdjson::dom::object node;
    EXPECT_SUCCESS(m_document["nodes"].at(index).get(node), "Failed to index node array")

    // TODO: Handle this.
    VULL_ENSURE(node["matrix"].error() == simdjson::error_code::NO_SUCH_FIELD);

    vull::Vec3f position;
    if (simdjson::dom::array position_array; node["translation"].get(position_array) == simdjson::SUCCESS) {
        if (!array_to_vec(position_array, position)) {
            return false;
        }
    }

    vull::Quatf rotation;
    if (simdjson::dom::array rotation_array; node["rotation"].get(rotation_array) == simdjson::SUCCESS) {
        if (!array_to_vec(rotation_array, rotation)) {
            return false;
        }
    }

    vull::Vec3f scale(1.0f);
    if (simdjson::dom::array scale_array; node["scale"].get(scale_array) == simdjson::SUCCESS) {
        if (!array_to_vec(scale_array, scale)) {
            return false;
        }
    }

    auto entity = m_world.create_entity();
    entity.add<vull::Transform>(parent_id, position, rotation, scale);
    if (uint64_t mesh_index; node["mesh"].get(mesh_index) == simdjson::SUCCESS) {
        std::string_view mesh_name_;
        EXPECT_SUCCESS(m_document["meshes"].at(mesh_index)["name"].get(mesh_name_), "Missing mesh name")
        vull::StringView mesh_name(mesh_name_.data(), mesh_name_.length());

        simdjson::dom::array primitives;
        EXPECT_SUCCESS(m_document["meshes"].at(mesh_index)["primitives"].get(primitives),
                       "Failed to get primitives array")
        if (primitives.size() == 1) {
            entity.add<vull::Mesh>(vull::format("/meshes/{}.0/vertex", mesh_name),
                                   vull::format("/meshes/{}.0/index", mesh_name));
            entity.add<vull::Material>("/default_albedo", "/default_normal");
        } else {
            for (size_t i = 0; i < primitives.size(); i++) {
                auto sub_entity = m_world.create_entity();
                sub_entity.add<vull::Transform>(entity);
                sub_entity.add<vull::Mesh>(vull::format("/meshes/{}.{}/vertex", mesh_name, i),
                                           vull::format("/meshes/{}.{}/index", mesh_name, i));
                sub_entity.add<vull::Material>("/default_albedo", "/default_normal");
            }
        }
    }

    if (simdjson::dom::array children; node["children"].get(children) == simdjson::SUCCESS) {
        for (auto child : children) {
            uint64_t child_index;
            EXPECT_SUCCESS(child.get(child_index), "Child node index not an integer")
            if (!visit_node(child_index, entity)) {
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

bool GltfParser::parse_glb(vull::StringView input_path) {
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

bool GltfParser::convert(vull::vpak::Writer &pack_writer, bool reproducible) {
    simdjson::dom::parser parser;
    simdjson::dom::element document;
    if (auto error = parser.parse(m_json.data(), m_json.length(), false).get(document)) {
        vull::error("[gltf] JSON parse error: {}", simdjson::error_message(error));
        return false;
    }

    simdjson::dom::object asset_info;
    if (auto error = document["asset"].get(asset_info)) {
        vull::error("[gltf] Failed to get asset info: {}", simdjson::error_message(error));
        return false;
    }
    if (std::string_view generator; asset_info["generator"].get(generator) == simdjson::SUCCESS) {
        vull::info("[gltf] Generator: {}", vull::StringView(generator.data(), generator.length()));
    }

    vull::World world;
    world.register_component<vull::Transform>();
    world.register_component<vull::Mesh>();
    world.register_component<vull::Material>();

    Converter converter(m_binary_blob, pack_writer, document, world);
    vull::Scheduler scheduler(reproducible ? 1 : 0);
    vull::Atomic<uint32_t> latch(1);
    vull::Atomic<bool> success = true;
    scheduler.start([&] {
        success.store(converter.convert(latch), vull::MemoryOrder::Relaxed);
    });
    do {
        syscall(SYS_futex, latch.raw_ptr(), FUTEX_WAIT_PRIVATE, 1, nullptr, nullptr, 0);
    } while (latch.load(vull::MemoryOrder::Relaxed) != 0);
    scheduler.stop();

    if (!success.load(vull::MemoryOrder::Relaxed)) {
        return false;
    }

    // The scene property may not be present, so we assume the first one if not.
    uint64_t scene_index = 0;
    VULL_IGNORE(document["scene"].get(scene_index));

    simdjson::dom::object scene;
    if (auto error = document["scenes"].at(scene_index).get(scene)) {
        vull::error("[gltf] Failed to get scene at index {}: {}", scene_index, simdjson::error_message(error));
        return false;
    }
    if (std::string_view scene_name; scene["name"].get(scene_name) == simdjson::SUCCESS) {
        vull::info("[gltf] Scene: {} (idx {})", vull::StringView(scene_name.data(), scene_name.length()), scene_index);
    }

    if (simdjson::dom::array root_nodes; scene["nodes"].get(root_nodes) == simdjson::SUCCESS) {
        for (auto node : root_nodes) {
            uint64_t index;
            EXPECT_SUCCESS(node.get(index), "Root node index not an integer")
            if (!converter.visit_node(index, ~vull::EntityId(0))) {
                return false;
            }
        }
    }

    world.serialise(pack_writer);
    vull::debug("[gltf] Finished traversing");
    return true;
}
