#include <vull/core/Mesh.hh>

#include <vull/support/Result.hh>
#include <vull/support/Stream.hh>
#include <vull/support/String.hh>
#include <vull/support/Utility.hh>

namespace vull {

Mesh Mesh::deserialise(Stream &stream) {
    auto vertex_data_name = VULL_EXPECT(stream.read_string());
    auto index_data_name = VULL_EXPECT(stream.read_string());
    return {vull::move(vertex_data_name), vull::move(index_data_name)};
}

void Mesh::serialise(Mesh &mesh, Stream &stream) {
    VULL_EXPECT(stream.write_string(mesh.vertex_data_name()));
    VULL_EXPECT(stream.write_string(mesh.index_data_name()));
}

} // namespace vull
