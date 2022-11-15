#include <vull/core/Material.hh>

#include <vull/support/Result.hh>
#include <vull/support/Stream.hh>
#include <vull/support/String.hh>
#include <vull/support/Utility.hh>

namespace vull {

Material Material::deserialise(Stream &stream) {
    auto albedo_name = VULL_EXPECT(stream.read_string());
    auto normal_name = VULL_EXPECT(stream.read_string());
    return {vull::move(albedo_name), vull::move(normal_name)};
}

void Material::serialise(Material &material, Stream &stream) {
    VULL_EXPECT(stream.write_string(material.albedo_name()));
    VULL_EXPECT(stream.write_string(material.normal_name()));
}

} // namespace vull
