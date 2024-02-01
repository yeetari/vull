#include <vull/graphics/material.hh>

#include <vull/support/result.hh>
#include <vull/support/stream.hh>
#include <vull/support/string.hh>
#include <vull/support/utility.hh>

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
