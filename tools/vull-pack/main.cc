#include <vull/io/PackFile.hh>
#include <vull/renderer/Vertex.hh>
#include <vull/support/Array.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Log.hh>
#include <vull/support/Vector.hh>

#define STB_DXT_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION
#include <glm/vec2.hpp>
#include <stb_dxt.h>
#include <stb_image.h>
#include <tiny_obj_loader.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <filesystem>
#include <istream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

void read(int argc, char **argv) {
    if (argc != 3) {
        std::printf("Usage: %s read <vpak>", argv[0]);
        return;
    }
    auto *input = std::fopen(argv[2], "r");
    PackFile pack(input);

    std::uint16_t entry_count = pack.read_header();
    Log::info("vull-pack", "Entry count: %u", entry_count);
    for (std::uint16_t i = 0; i < entry_count; i++) {
        auto entry = pack.read_entry();
        Log::info("vull-pack", "Entry %u; type %u (%s); payload_size %lu", i, entry.type(),
                  PackFile::entry_type_str(entry.type()), entry.payload_size());
        if (!entry.name().empty()) {
            Log::info("vull-pack", "  Name: %s", entry.name().c_str());
        }
        if (entry.type() == PackEntryType::Mesh) {
            auto data = pack.read_data(entry);
            PackMesh mesh(data);
            Log::info("vull-pack", "  Index count: %u", mesh.index_count());
            Log::info("vull-pack", "  Index offset: %lu", mesh.index_offset());
        } else if (entry.type() == PackEntryType::Texture) {
            auto data = pack.read_data(entry);
            PackTexture texture(data);
            Log::info("vull-pack", "  Width: %u", texture.width());
            Log::info("vull-pack", "  Height: %u", texture.height());
        } else {
            pack.skip_data(entry);
        }
    }
    std::fclose(input);
}

// NOLINTNEXTLINE
void write(int argc, char **argv) {
    if (argc != 4) {
        std::printf("Usage: %s write <vpak> <directory>\n", argv[0]);
        return;
    }
    auto *output = std::fopen(argv[2], "w");
    PackFile pack(output);

    struct InputFile {
        std::string path;
        std::string name;
    };
    Vector<InputFile> obj_inputs;
    Vector<InputFile> spv_inputs;
    Vector<InputFile> tex_inputs;

    std::string directory(argv[3]);
    for (const auto &entry : std::filesystem::recursive_directory_iterator(directory)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        auto extension = entry.path().extension();
        auto name = entry.path().lexically_normal().parent_path().string();
        auto slash_index = name.find_last_of('/');
        if (slash_index != std::string::npos) {
            name = name.substr(slash_index);
        }
        name += '/' + entry.path().stem().string();
        if (extension == ".obj") {
            obj_inputs.push(InputFile{
                .path = entry.path().string(),
                .name = std::move(name),
            });
        } else if (extension == ".spv") {
            spv_inputs.push(InputFile{
                .path = entry.path().string(),
                .name = std::move(name),
            });
        } else if (extension == ".jpg" || extension == ".png") {
            tex_inputs.push(InputFile{
                .path = entry.path().string(),
                .name = std::move(name),
            });
        }
    }

    std::uint16_t entry_count = obj_inputs.size() + spv_inputs.size() + tex_inputs.size();
    if (!obj_inputs.empty()) {
        entry_count += 2;
    }
    pack.write_header(entry_count);

    if (!obj_inputs.empty()) {
        Vector<Vertex> vertices;
        Vector<std::uint32_t> indices;
        std::unordered_map<Vertex, std::uint32_t> unique_vertices;
        std::uint64_t running_offset = 0;
        for (const auto &input : obj_inputs) {
            tinyobj::ObjReader reader;
            ENSURE(reader.ParseFromFile(input.path));

            const auto &attrib = reader.GetAttrib();
            std::uint32_t index_count = 0;
            for (const auto &shape : reader.GetShapes()) {
                index_count += shape.mesh.indices.size();
                for (const auto &index : shape.mesh.indices) {
                    Vertex vertex{
                        .position{
                            attrib.vertices[index.vertex_index * 3 + 0],
                            attrib.vertices[index.vertex_index * 3 + 1],
                            attrib.vertices[index.vertex_index * 3 + 2],
                        },
                        .normal{
                            attrib.normals[index.normal_index * 3 + 0],
                            attrib.normals[index.normal_index * 3 + 1],
                            attrib.normals[index.normal_index * 3 + 2],
                        },
                    };
                    if (!attrib.texcoords.empty()) {
                        vertex.uv = {
                            attrib.texcoords[index.texcoord_index * 2 + 0],
                            attrib.texcoords[index.texcoord_index * 2 + 1],
                        };
                    }
                    if (!unique_vertices.contains(vertex)) {
                        unique_vertices.emplace(vertex, vertices.size());
                        vertices.push(vertex);
                    }
                    indices.push(unique_vertices.at(vertex));
                }
            }

            Array<std::uint8_t, 12> mesh_bytes{
                static_cast<std::uint8_t>(index_count >> 0u & 0xffu),
                static_cast<std::uint8_t>(index_count >> 8u & 0xffu),
                static_cast<std::uint8_t>(index_count >> 16u & 0xffu),
                static_cast<std::uint8_t>(index_count >> 24u & 0xffu),
                static_cast<std::uint8_t>(running_offset >> 0u & 0xffu),
                static_cast<std::uint8_t>(running_offset >> 8u & 0xffu),
                static_cast<std::uint8_t>(running_offset >> 16u & 0xffu),
                static_cast<std::uint8_t>(running_offset >> 24u & 0xffu),
                static_cast<std::uint8_t>(running_offset >> 32u & 0xffu),
                static_cast<std::uint8_t>(running_offset >> 40u & 0xffu),
                static_cast<std::uint8_t>(running_offset >> 48u & 0xffu),
                static_cast<std::uint8_t>(running_offset >> 56u & 0xffu),
            };
            pack.write_entry_header(PackEntryType::Mesh, input.name.length() + mesh_bytes.size());
            pack.write({reinterpret_cast<const std::uint8_t *>(input.name.data()), input.name.length()});
            pack.write(mesh_bytes);
            running_offset += index_count;
        }

        pack.write_entry_header(PackEntryType::VertexBuffer, vertices.size_bytes());
        pack.write({reinterpret_cast<std::uint8_t *>(vertices.data()), vertices.size_bytes()});

        pack.write_entry_header(PackEntryType::IndexBuffer, indices.size_bytes());
        pack.write({reinterpret_cast<std::uint8_t *>(indices.data()), indices.size_bytes()});
    }

    for (const auto &input : spv_inputs) {
        std::ifstream file(input.path, std::ios::ate | std::ios::binary);
        ENSURE(file);
        Vector<std::uint8_t> buffer(file.tellg());
        file.seekg(0);
        file.read(reinterpret_cast<char *>(buffer.data()), buffer.capacity());
        pack.write_entry_header(PackEntryType::Shader, buffer.size_bytes() + input.name.length() + 1);
        pack.write({reinterpret_cast<const std::uint8_t *>(input.name.c_str()), input.name.length() + 1});
        pack.write(buffer);
    }

    for (const auto &input : tex_inputs) {
        std::uint32_t width;
        std::uint32_t height;
        auto *image = stbi_load(input.path.c_str(), reinterpret_cast<int *>(&width), reinterpret_cast<int *>(&height),
                                nullptr, STBI_rgb_alpha);
        ENSURE(image != nullptr);

        const std::uint32_t x_block_count = width / 4;
        const std::uint32_t y_block_count = height / 4;
        Vector<std::uint8_t> compressed_image(x_block_count * y_block_count * 16);
        for (std::uint32_t y_block = 0; y_block < y_block_count; y_block++) {
            for (std::uint32_t x_block = 0; x_block < x_block_count; x_block++) {
                // NOLINTNEXTLINE
                Array<std::uint8_t, 64> src;
                for (std::uint32_t y = 0; y < 4; y++) {
                    std::memcpy(src.data() + y * 16, image + ((y_block * width * 4) + (y * width) + (x_block * 4)) * 4,
                                16);
                }
                stb_compress_dxt_block(compressed_image.data() + (y_block * x_block_count + x_block) * 16, src.data(),
                                       1, STB_DXT_HIGHQUAL);
            }
        }
        stbi_image_free(image);

        Array<std::uint8_t, 8> texture_bytes{
            static_cast<std::uint8_t>(width >> 0u & 0xffu),   static_cast<std::uint8_t>(width >> 8u & 0xffu),
            static_cast<std::uint8_t>(width >> 16u & 0xffu),  static_cast<std::uint8_t>(width >> 24u & 0xffu),
            static_cast<std::uint8_t>(height >> 0u & 0xffu),  static_cast<std::uint8_t>(height >> 8u & 0xffu),
            static_cast<std::uint8_t>(height >> 16u & 0xffu), static_cast<std::uint8_t>(height >> 24u & 0xffu),
        };
        pack.write_entry_header(PackEntryType::Texture, compressed_image.size_bytes() + input.name.length() + 9);
        pack.write({reinterpret_cast<const std::uint8_t *>(input.name.c_str()), input.name.length() + 1});
        pack.write(texture_bytes);
        pack.write(compressed_image);
    }
    std::fflush(output);
    std::fclose(output);
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <read/write>\n", argv[0]);
        return 0;
    }
    std::string mode(argv[1]);
    try {
        if (mode == "read") {
            read(argc, argv);
        } else if (mode == "write") {
            write(argc, argv);
        }
    } catch (const std::exception &exception) {
        fprintf(stderr, "%s\n", exception.what());
        return 1;
    }
}
