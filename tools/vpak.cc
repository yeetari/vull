#include "TextureLoader.hh"

#include <vull/core/Material.hh>
#include <vull/core/Mesh.hh>
#include <vull/core/Transform.hh>
#include <vull/ecs/World.hh>
#include <vull/maths/Vec.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Format.hh>
#include <vull/support/Optional.hh>
#include <vull/support/Vector.hh>
#include <vull/vpak/PackFile.hh>
#include <vull/vpak/PackWriter.hh>
#include <vull/vulkan/Vulkan.hh>

#include <assimp/DefaultLogger.hpp>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <libgen.h>
#include <meshoptimizer.h>
#include <time.h>

using namespace vull;

namespace {

struct AssimpLogger : public Assimp::LogStream {
    void write(const char *message) override { fputs(message, stdout); }
};

struct Vertex {
    Vec3f position;
    Vec3f normal;
    Vec2f uv;
};

double get_time() {
    struct timespec ts {};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<double>(static_cast<uint64_t>(ts.tv_sec) * 1000000000 + static_cast<uint64_t>(ts.tv_nsec)) /
           1000000000;
}

void process_material(PackWriter &pack_writer, const char *root_path, const aiMaterial *material, int indentation) {
    if (const auto count = material->GetTextureCount(aiTextureType_DIFFUSE); count != 0) {
        VULL_ENSURE(count == 1);
    } else {
        // TODO: Can this ever != 0?
        VULL_ENSURE(material->GetTextureCount(aiTextureType_BASE_COLOR) == 0);
        return;
    }

    aiString albedo_path;
    material->GetTexture(aiTextureType_DIFFUSE, 0, &albedo_path);
    auto path = vull::format("{}/{}", root_path, albedo_path.C_Str());

    // TODO: Horrible hack to replace backslashes with forward slashes.
    char *slash_ptr = nullptr;
    while ((slash_ptr = strchr(path.data(), '\\')) != nullptr) {
        *slash_ptr = '/';
    }

    VULL_ENSURE(load_texture(pack_writer, path));
    float ratio = pack_writer.end_entry();
    printf("%*s(%s): %.1f%%\n", indentation + 2, "", albedo_path.C_Str(), ratio * 100.0f);
}

void process_mesh(const aiMesh *mesh, Vector<Vertex> &vertices, Vector<uint32_t> &indices) {
    vertices.ensure_capacity(mesh->mNumVertices);
    for (unsigned i = 0; i < mesh->mNumVertices; i++) {
        vertices.push(Vertex{
            .position{
                mesh->mVertices[i].x,
                mesh->mVertices[i].y,
                mesh->mVertices[i].z,
            },
            .normal = normalise(Vec3f{
                mesh->mNormals[i].x,
                mesh->mNormals[i].y,
                mesh->mNormals[i].z,
            }),
            .uv{
                mesh->mTextureCoords[0][i].x,
                mesh->mTextureCoords[0][i].y,
            },
        });
    }
    indices.ensure_capacity(mesh->mNumFaces * 3);
    for (unsigned i = 0; i < mesh->mNumFaces; i++) {
        const auto &face = mesh->mFaces[i];
        indices.push(face.mIndices[0]);
        indices.push(face.mIndices[1]);
        indices.push(face.mIndices[2]);
    }
}

void process_node(const char *root_path, const aiScene *scene, EntityManager &world, PackWriter &pack_writer,
                  Vector<Optional<Material>> &materials, EntityId parent_id, aiNode *node, uint32_t &mesh_index,
                  uint32_t &texture_index, int indentation) {
    printf("%*s%s\n", indentation, "", node->mName.C_Str());

    // Create a container entity that acts as a parent for any meshes, and that any child nodes can use as parent.
    // TODO: A more optimal way to handle multiple meshes on a node?
    // TODO: At least don't generate a container entity for single-mesh nodes.
    auto container_entity = world.create_entity();
    container_entity.add<Transform>(parent_id, Mat4f{Array{
                                                   Vec4f(node->mTransformation.a1, node->mTransformation.b1,
                                                         node->mTransformation.c1, node->mTransformation.d1),
                                                   Vec4f(node->mTransformation.a2, node->mTransformation.b2,
                                                         node->mTransformation.c2, node->mTransformation.d2),
                                                   Vec4f(node->mTransformation.a3, node->mTransformation.b3,
                                                         node->mTransformation.c3, node->mTransformation.d3),
                                                   Vec4f(node->mTransformation.a4, node->mTransformation.b4,
                                                         node->mTransformation.c4, node->mTransformation.d4),
                                               }});
    for (unsigned i = 0; i < node->mNumMeshes; i++) {
        auto entity = world.create_entity();
        entity.add<Transform>(container_entity, Mat4f(1.0f));

        Vector<Vertex> vertices;
        Vector<uint32_t> indices;
        const auto *mesh = scene->mMeshes[node->mMeshes[i]];
        process_mesh(mesh, vertices, indices);
        entity.add<Mesh>(mesh_index++, indices.size());

        meshopt_optimizeVertexCache(indices.data(), indices.data(), indices.size(), vertices.size());
        meshopt_optimizeVertexFetch(vertices.data(), indices.data(), indices.size(), vertices.data(), vertices.size(),
                                    sizeof(Vertex));

        pack_writer.start_entry(PackEntryType::VertexData, true);
        pack_writer.write(vertices.span());
        float vertex_ratio = pack_writer.end_entry();

        pack_writer.start_entry(PackEntryType::IndexData, indices.size() > 6);
        pack_writer.write(indices.span());
        float index_ratio = pack_writer.end_entry();

        printf("%*s(mesh %u): %.1f%% verts, %.1f%% inds\n", indentation + 2, "", i, vertex_ratio * 100.0f,
               index_ratio * 100.0f);

        if (!materials[mesh->mMaterialIndex]) {
            process_material(pack_writer, root_path, scene->mMaterials[mesh->mMaterialIndex], indentation + 2);
            materials[mesh->mMaterialIndex].emplace(texture_index++);
        }
        entity.add<Material>(*materials[mesh->mMaterialIndex]);
    }

    // Recursively traverse children.
    for (unsigned i = 0; i < node->mNumChildren; i++) {
        process_node(root_path, scene, world, pack_writer, materials, container_entity, node->mChildren[i], mesh_index,
                     texture_index, indentation + 2);
    }
}

} // namespace

int main(int, char **argv) {
    auto start_time = get_time();
    Assimp::DefaultLogger::create("vpak", Assimp::Logger::VERBOSE);
    Assimp::DefaultLogger::get()->attachStream(new AssimpLogger, Assimp::Logger::Debugging | Assimp::Logger::Info |
                                                                     Assimp::Logger::Warn | Assimp::Logger::Err);
    Assimp::Importer importer;
    importer.SetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS, aiComponent_TANGENTS_AND_BITANGENTS | aiComponent_COLORS |
                                                            aiComponent_BONEWEIGHTS | aiComponent_ANIMATIONS |
                                                            aiComponent_LIGHTS | aiComponent_CAMERAS);
    const auto *scene = importer.ReadFile(argv[1], aiProcess_RemoveComponent | aiProcess_Triangulate |
                                                       aiProcess_SortByPType | aiProcess_JoinIdenticalVertices |
                                                       aiProcess_FlipUVs | aiProcess_ValidateDataStructure);
    if (scene == nullptr || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0 ||
        (scene->mFlags & AI_SCENE_FLAGS_VALIDATION_WARNING) != 0) {
        return 1;
    }
    putchar('\n');

    World world;
    world.register_component<Transform>();
    world.register_component<Mesh>();
    world.register_component<Material>();

    auto *pack_file = fopen("scene.vpak", "wb");
    PackWriter pack_writer(pack_file);
    pack_writer.write_header();

    // Walk imported scene hierarchy.
    const char *root_path = dirname(argv[1]);
    Vector<Optional<Material>> materials(scene->mNumMaterials);
    uint32_t mesh_index = 0;
    uint32_t texture_index = 0;
    process_node(root_path, scene, world, pack_writer, materials, 0, scene->mRootNode, mesh_index, texture_index, 0);

    // Serialise ECS state.
    float world_ratio = world.serialise(pack_writer);
    printf("(world): %.1f%%\n", world_ratio * 100.0f);

    printf("\nWrote %ld bytes in %.2f seconds\n", ftell(pack_file), get_time() - start_time);
    fclose(pack_file);
    Assimp::DefaultLogger::kill();
}
