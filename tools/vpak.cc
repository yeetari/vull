#include <vull/maths/Vec.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Vector.hh>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <meshoptimizer.h>

using namespace vull;

namespace {

struct Vertex {
    Vec3f position;
    Vec3f normal;
};

FILE *s_vertex_file;
FILE *s_index_file;

void process_mesh(aiMesh *mesh, Vector<Vertex> &vertices, Vector<uint32_t> &indices, uint32_t &vertex_count) {
    VULL_ENSURE(mesh->mVertices != nullptr);
    VULL_ENSURE(mesh->mNormals != nullptr);
    for (unsigned i = 0; i < mesh->mNumVertices; i++) {
        Vertex vertex{
            .position{
                mesh->mVertices[i].x,
                mesh->mVertices[i].z,
                mesh->mVertices[i].y,
            },
            .normal = normalise(Vec3f{
                mesh->mNormals[i].x,
                mesh->mNormals[i].z,
                mesh->mNormals[i].y,
            }),
        };
        vertices.push(vertex);
    }
    for (unsigned i = 0; i < mesh->mNumFaces; i++) {
        const auto &face = mesh->mFaces[i];
        indices.push(vertex_count + face.mIndices[0]);
        indices.push(vertex_count + face.mIndices[1]);
        indices.push(vertex_count + face.mIndices[2]);
    }
    vertex_count += mesh->mNumVertices;
}

void process_node(const aiScene *scene, aiNode *node, Vector<Vertex> &vertices, Vector<uint32_t> &indices,
                  uint32_t &vertex_count) {
    for (unsigned i = 0; i < node->mNumMeshes; i++) {
        process_mesh(scene->mMeshes[node->mMeshes[i]], vertices, indices, vertex_count);
    }
    for (unsigned i = 0; i < node->mNumChildren; i++) {
        process_node(scene, node->mChildren[i], vertices, indices, vertex_count);
    }
}

} // namespace

int main(int, char **argv) {
    Assimp::Importer importer;
    importer.SetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS,
                                aiComponent_TANGENTS_AND_BITANGENTS | aiComponent_COLORS | aiComponent_TEXCOORDS |
                                    aiComponent_BONEWEIGHTS | aiComponent_ANIMATIONS | aiComponent_TEXTURES |
                                    aiComponent_LIGHTS | aiComponent_CAMERAS | aiComponent_MATERIALS);
    const auto *scene =
        importer.ReadFile(argv[1], aiProcess_RemoveComponent | aiProcess_Triangulate | aiProcess_SortByPType |
                                       aiProcess_JoinIdenticalVertices | aiProcess_ValidateDataStructure);
    VULL_ENSURE(scene != nullptr);

    Vector<Vertex> vertices;
    Vector<uint32_t> indices;
    uint32_t vertex_count = 0;
    process_node(scene, scene->mRootNode, vertices, indices, vertex_count);
    meshopt_optimizeVertexCache(indices.data(), indices.data(), indices.size(), vertex_count);
    meshopt_optimizeVertexFetch(vertices.data(), indices.data(), indices.size(), vertices.data(), vertices.size(),
                                sizeof(Vertex));

    s_vertex_file = fopen("vertices", "wb");
    s_index_file = fopen("indices", "wb");
    fwrite(vertices.data(), sizeof(Vertex), vertices.size(), s_vertex_file);
    fwrite(indices.data(), sizeof(uint32_t), indices.size(), s_index_file);
    fclose(s_index_file);
    fclose(s_vertex_file);
}
