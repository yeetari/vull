#include <vull/core/Material.hh>
#include <vull/core/Mesh.hh>
#include <vull/core/Transform.hh>
#include <vull/core/Vertex.hh>
#include <vull/ecs/World.hh>
#include <vull/maths/Vec.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Timer.hh>
#include <vull/support/Vector.hh>
#include <vull/vpak/PackFile.hh>
#include <vull/vpak/PackWriter.hh>

#include <meshoptimizer.h>
#include <simdjson.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define DWORD_LE(data, start)                                                                                          \
    (static_cast<uint32_t>((data)[(start)] << 0u) | static_cast<uint32_t>((data)[(start) + 1] << 8u) |                 \
     static_cast<uint32_t>((data)[(start) + 2] << 16u) | static_cast<uint32_t>((data)[(start) + 3] << 24u))

namespace {

class Traverser {
    struct MeshInfo {
        uint32_t index_count;
        float vertex_ratio;
        float index_ratio;
    };

    const uint8_t *const m_binary_data;
    simdjson::dom::element &m_document;
    vull::PackWriter &m_pack_writer;
    vull::World &m_world;
    const bool m_fast;

    vull::Vector<vull::Material> m_materials;
    vull::Vector<MeshInfo> m_meshes;

public:
    Traverser(const uint8_t *binary_data, simdjson::dom::element &document, vull::PackWriter &pack_writer,
              vull::World &world, bool fast)
        : m_binary_data(binary_data), m_document(document), m_pack_writer(pack_writer), m_world(world), m_fast(fast) {}

    void process_materials();
    void process_meshes();
    void process_primitive(simdjson::dom::object &primitive);
    void traverse_node(uint64_t node_index, vull::EntityId parent_id, int indentation);
};

void Traverser::process_materials() {
    simdjson::dom::array materials;
    if (m_document["materials"].get(materials) != simdjson::SUCCESS) {
        return;
    }
    for (auto material : materials) {
        static_cast<void>(material); // TODO
        // Emit error albedo.
        m_pack_writer.start_entry(vull::PackEntryType::ImageData, true);
        m_pack_writer.write_byte(uint8_t(vull::PackImageFormat::RgbaUnorm));
        m_pack_writer.write_varint(16);
        m_pack_writer.write_varint(16);
        m_pack_writer.write_varint(1);
        constexpr vull::Array colours{
            vull::Vec<uint8_t, 4>(0xff, 0x69, 0xb4, 0xff),
            vull::Vec<uint8_t, 4>(0x94, 0x00, 0xd3, 0xff),
        };
        for (uint32_t y = 0; y < 16; y++) {
            for (uint32_t x = 0; x < 16; x++) {
                uint32_t colour_index = (x + y) % colours.size();
                m_pack_writer.write({&colours[colour_index], 4});
            }
        }
        m_pack_writer.end_entry();

        // Emit default normal map.
        m_pack_writer.start_entry(vull::PackEntryType::ImageData, true);
        m_pack_writer.write_byte(uint8_t(vull::PackImageFormat::RgUnorm));
        m_pack_writer.write_varint(1);
        m_pack_writer.write_varint(1);
        m_pack_writer.write_varint(1);
        constexpr vull::Array<uint8_t, 2> data{128, 128};
        m_pack_writer.write(data.span());
        m_pack_writer.end_entry();

        m_materials.emplace(m_materials.size() * 2, m_materials.size() * 2 + 1);
    }
}

void Traverser::process_meshes() {
    simdjson::dom::array meshes;
    VULL_ENSURE(m_document["meshes"].get(meshes) == simdjson::SUCCESS);
    for (auto mesh : meshes) {
        simdjson::dom::array primitives;
        VULL_ENSURE(mesh["primitives"].get(primitives) == simdjson::SUCCESS);
        for (auto prim : primitives) {
            simdjson::dom::object primitive;
            VULL_ENSURE(prim.get(primitive) == simdjson::SUCCESS);
            process_primitive(primitive);
        }
    }
}

void Traverser::process_primitive(simdjson::dom::object &primitive) {
    uint64_t positions_index;
    VULL_ENSURE(primitive["attributes"]["POSITION"].get(positions_index) == simdjson::SUCCESS);

    uint64_t normals_index;
    VULL_ENSURE(primitive["attributes"]["NORMAL"].get(normals_index) == simdjson::SUCCESS);

    uint64_t uvs_index;
    VULL_ENSURE(primitive["attributes"]["TEXCOORD_0"].get(uvs_index) == simdjson::SUCCESS);

    uint64_t indices_index;
    VULL_ENSURE(primitive["indices"].get(indices_index) == simdjson::SUCCESS);

    // TODO: Accessor validation.
    auto positions_accessor = m_document["accessors"].at(positions_index);
    auto normals_accessor = m_document["accessors"].at(normals_index);
    auto uvs_accessor = m_document["accessors"].at(uvs_index);
    auto indices_accessor = m_document["accessors"].at(indices_index);

    VULL_ENSURE(positions_accessor["bufferView"].get(positions_index) == simdjson::SUCCESS);
    VULL_ENSURE(normals_accessor["bufferView"].get(normals_index) == simdjson::SUCCESS);
    VULL_ENSURE(uvs_accessor["bufferView"].get(uvs_index) == simdjson::SUCCESS);
    VULL_ENSURE(indices_accessor["bufferView"].get(indices_index) == simdjson::SUCCESS);

    uint64_t positions_offset;
    VULL_ENSURE(m_document["bufferViews"].at(positions_index)["byteOffset"].get(positions_offset) == simdjson::SUCCESS);

    uint64_t normals_offset;
    VULL_ENSURE(m_document["bufferViews"].at(normals_index)["byteOffset"].get(normals_offset) == simdjson::SUCCESS);

    uint64_t uvs_offset;
    VULL_ENSURE(m_document["bufferViews"].at(uvs_index)["byteOffset"].get(uvs_offset) == simdjson::SUCCESS);

    uint64_t indices_offset;
    VULL_ENSURE(m_document["bufferViews"].at(indices_index)["byteOffset"].get(indices_offset) == simdjson::SUCCESS);

    uint64_t vertex_count;
    VULL_ENSURE(positions_accessor["count"].get(vertex_count) == simdjson::SUCCESS);
    vull::Vector<vull::Vertex> vertices;
    vertices.ensure_capacity(static_cast<uint32_t>(vertex_count));
    for (uint64_t i = 0; i < vertex_count; i++) {
        auto &vertex = vertices.emplace();
        memcpy(&vertex.position, &m_binary_data[positions_offset + i * sizeof(vull::Vec3f)], sizeof(vull::Vec3f));
        memcpy(&vertex.normal, &m_binary_data[normals_offset + i * sizeof(vull::Vec3f)], sizeof(vull::Vec3f));
        memcpy(&vertex.uv, &m_binary_data[uvs_offset + i * sizeof(vull::Vec2f)], sizeof(vull::Vec2f));
    }

    uint64_t index_count;
    VULL_ENSURE(indices_accessor["count"].get(index_count) == simdjson::SUCCESS);

    uint64_t index_type;
    VULL_ENSURE(indices_accessor["componentType"].get(index_type) == simdjson::SUCCESS);

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
        VULL_ENSURE_NOT_REACHED();
    }

    vull::Vector<uint32_t> indices(static_cast<uint32_t>(index_count));
    for (auto &index : indices) {
        const auto *index_bytes = &m_binary_data[indices_offset];
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
            __builtin_unreachable();
        }
        indices_offset += index_size;
    }

    if (!m_fast) {
        meshopt_optimizeVertexCache(indices.data(), indices.data(), indices.size(), vertices.size());
        meshopt_optimizeVertexFetch(vertices.data(), indices.data(), indices.size(), vertices.data(), vertices.size(),
                                    sizeof(vull::Vertex));
    }

    m_pack_writer.start_entry(vull::PackEntryType::VertexData, true);
    m_pack_writer.write(vertices.span());
    float vertex_ratio = m_pack_writer.end_entry();

    m_pack_writer.start_entry(vull::PackEntryType::IndexData, indices.size() > 6);
    m_pack_writer.write(indices.span());
    float index_ratio = m_pack_writer.end_entry();
    m_meshes.push(MeshInfo{indices.size(), vertex_ratio, index_ratio});
}

void Traverser::traverse_node(uint64_t node_index, vull::EntityId parent_id, int indentation) {
    simdjson::dom::object node;
    VULL_ENSURE(m_document["nodes"].at(node_index).get(node) == simdjson::SUCCESS);

    std::string_view node_name = "<unnamed>";
    static_cast<void>(node["name"].get(node_name));
    printf("%*s%.*s\n", indentation, "", static_cast<int>(node_name.length()), node_name.data());

    vull::Vec<double, 3> position(0.0);
    vull::Quat<double> rotation;
    vull::Vec<double, 3> scale(1.0);
    if (simdjson::dom::array position_array; node["translation"].get(position_array) == simdjson::SUCCESS) {
        VULL_ENSURE(position_array.at(0).get(position[0]) == simdjson::SUCCESS);
        VULL_ENSURE(position_array.at(1).get(position[1]) == simdjson::SUCCESS);
        VULL_ENSURE(position_array.at(2).get(position[2]) == simdjson::SUCCESS);
    }
    if (simdjson::dom::array rotation_array; node["rotation"].get(rotation_array) == simdjson::SUCCESS) {
        VULL_ENSURE(rotation_array.at(0).get(rotation[0]) == simdjson::SUCCESS);
        VULL_ENSURE(rotation_array.at(1).get(rotation[1]) == simdjson::SUCCESS);
        VULL_ENSURE(rotation_array.at(2).get(rotation[2]) == simdjson::SUCCESS);
        VULL_ENSURE(rotation_array.at(3).get(rotation[3]) == simdjson::SUCCESS);
    }
    if (simdjson::dom::array scale_array; node["scale"].get(scale_array) == simdjson::SUCCESS) {
        VULL_ENSURE(scale_array.at(0).get(scale[0]) == simdjson::SUCCESS);
        VULL_ENSURE(scale_array.at(1).get(scale[1]) == simdjson::SUCCESS);
        VULL_ENSURE(scale_array.at(2).get(scale[2]) == simdjson::SUCCESS);
    }

    // Create a container entity that acts as a parent for any primitives, and that any child nodes can use as parent.
    // TODO: A more optimal way to handle multiple primitives on a node?
    // TODO: At least don't generate a container entity for single-primitive nodes.
    vull::Quatf rotation_float(rotation.x(), rotation.y(), rotation.z(), rotation.w());
    auto container_entity = m_world.create_entity();
    container_entity.add<vull::Transform>(parent_id, position, rotation_float, scale);
    if (uint64_t mesh_index; node["mesh"].get(mesh_index) == simdjson::SUCCESS) {
        simdjson::dom::array primitives;
        VULL_ENSURE(m_document["meshes"].at(mesh_index)["primitives"].get(primitives) == simdjson::SUCCESS);
        for (uint32_t i = 0; auto primitive : primitives) {
            const auto mesh_array_index = static_cast<uint32_t>(mesh_index) + i++;
            const auto &mesh_info = m_meshes[mesh_array_index];
            printf("%*s(mesh %u): %.1f%% verts, %.1f%% inds\n", indentation + 2, "", mesh_array_index,
                   mesh_info.vertex_ratio, mesh_info.index_ratio);

            auto entity = m_world.create_entity();
            entity.add<vull::Transform>(container_entity);
            entity.add<vull::Mesh>(mesh_array_index, mesh_info.index_count);
            uint64_t material_index = 0;
            static_cast<void>(primitive["material"].get(material_index));
            entity.add<vull::Material>(m_materials[static_cast<uint32_t>(material_index)]);
        }
    }

    if (simdjson::dom::array children_indices; node["children"].get(children_indices) == simdjson::SUCCESS) {
        for (auto child_index : children_indices) {
            traverse_node(child_index.get_uint64().value_unsafe(), container_entity, indentation + 2);
        }
    }
}

void print_usage(const char *executable) {
    fprintf(stderr, "usage: %s [--dump-json] [--fast|--ultra] <input-gltf> [output-vpak]\n", executable);
}

} // namespace

int main(int argc, char **argv) {
    bool dump_json = false;
    bool fast = false;
    bool ultra = false;
    const char *input_path = nullptr;
    const char *output_path = nullptr;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--dump-json") == 0) {
            dump_json = true;
        } else if (strcmp(argv[i], "--fast") == 0) {
            fast = true;
        } else if (strcmp(argv[i], "--ultra") == 0) {
            ultra = true;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "fatal: unknown option %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        } else if (input_path == nullptr) {
            input_path = argv[i];
        } else if (output_path == nullptr) {
            output_path = argv[i];
        } else {
            fprintf(stderr, "fatal: invalid argument %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    if (fast && ultra) {
        fputs("fatal: can't have --fast and --ultra\n", stderr);
        return 1;
    }
    if (input_path == nullptr) {
        print_usage(argv[0]);
        return 1;
    }
    output_path = output_path != nullptr ? output_path : "scene.vpak";

    auto compression_level = vull::CompressionLevel::Normal;
    if (fast) {
        compression_level = vull::CompressionLevel::None;
    }
    if (ultra) {
        compression_level = vull::CompressionLevel::Ultra;
    }

    // TODO: Tidy this block up (file/mmap utility in vull + proper error handling).
    FILE *file = fopen(input_path, "rb");
    struct stat stat {};
    fstat(fileno(file), &stat);
    const auto file_size = static_cast<size_t>(stat.st_size);
    const auto *data = static_cast<const uint8_t *>(mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, fileno(file), 0));
    fclose(file);

    vull::Timer timer;
    FILE *pack_file = fopen(output_path, "wb");
    vull::PackWriter pack_writer(pack_file, compression_level);
    pack_writer.write_header();

    // Validate magic number.
    if (const auto magic = DWORD_LE(data, 0); magic != 0x46546c67u) {
        fprintf(stderr, "fatal: invalid magic number 0x%x\n", magic);
        return 1;
    }

    // Validate version.
    if (const auto version = DWORD_LE(data, 4); version != 2) {
        fprintf(stderr, "fatal: unsupported version %u\n", version);
        return 1;
    }

    // Validate the alleged size in the header is the actual size.
    if (const auto size = DWORD_LE(data, 8); size != file_size) {
        fprintf(stderr, "fatal: size mismatch %u vs %lu\n", size, file_size);
        return 1;
    }

    // glTF 2 must have a single JSON chunk at the start.
    uint32_t json_length = DWORD_LE(data, 12);
    if (DWORD_LE(data, 16) != 0x4e4f534au) {
        fputs("fatal: missing or invalid JSON chunk\n", stderr);
        return 1;
    }

    if (dump_json) {
        printf("%.*s\n", json_length, &data[20]);
        return 0;
    }

    simdjson::dom::parser parser;
    simdjson::dom::element document;
    if (auto error = parser.parse(&data[20], json_length, false).get(document)) {
        fprintf(stderr, "fatal: json parse error (%s)\n", simdjson::error_message(error));
        return 1;
    }

    simdjson::dom::object asset_info;
    if (auto error = document["asset"].get(asset_info)) {
        fprintf(stderr, "fatal: failed to get asset info (%s)\n", simdjson::error_message(error));
        return 1;
    }
    if (std::string_view generator; asset_info["generator"].get(generator) == simdjson::SUCCESS) {
        printf("Generator: %.*s\n", static_cast<int>(generator.length()), generator.data());
    }

    vull::World world;
    world.register_component<vull::Transform>();
    world.register_component<vull::Mesh>();
    world.register_component<vull::Material>();

    Traverser traverser(&data[20 + json_length + 8], document, pack_writer, world, fast);
    traverser.process_materials();
    traverser.process_meshes();

    // The scene property may not be present, so we assume the first one if not.
    uint64_t scene_index = 0;
    static_cast<void>(document["scene"].get(scene_index));

    simdjson::dom::object scene;
    if (auto error = document["scenes"].at(scene_index).get(scene)) {
        fprintf(stderr, "fatal: failed to get scene at index %lu (%s)\n", scene_index, simdjson::error_message(error));
        return 1;
    }
    if (std::string_view scene_name; scene["name"].get(scene_name) == simdjson::SUCCESS) {
        printf("Scene: \"%.*s\" (idx %lu)\n", static_cast<int>(scene_name.length()), scene_name.data(), scene_index);
    }

    auto root_entity = world.create_entity();
    root_entity.add<vull::Transform>(root_entity);
    if (simdjson::dom::array node_indices; scene["nodes"].get(node_indices) == simdjson::SUCCESS) {
        for (auto node_index : node_indices) {
            traverser.traverse_node(node_index.get_uint64().value_unsafe(), root_entity, 0);
        }
    }

    // Serialise ECS state.
    float world_ratio = world.serialise(pack_writer);
    printf("(world): %.1f%%\n", world_ratio);

    printf("\nWrote %lu bytes to %s in %.2f seconds\n", ftello(pack_file), output_path, timer.elapsed());
    fclose(pack_file);
}
