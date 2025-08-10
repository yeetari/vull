#include <vull/graphics/mesh.hh>

#include <vull/support/result.hh>
#include <vull/support/stream.hh>
#include <vull/support/string.hh>
#include <vull/support/utility.hh>

namespace vull {

Mesh Mesh::deserialise(Stream &stream) {
    auto data_path = VULL_EXPECT(stream.read_string());
    return Mesh(vull::move(data_path));
}

void Mesh::serialise(Mesh &mesh, Stream &stream) {
    VULL_EXPECT(stream.write_string(mesh.data_path()));
}

} // namespace vull
