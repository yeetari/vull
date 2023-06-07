#pragma once

#include <vull/vulkan/RenderGraphDefs.hh>

namespace vull {

struct GBuffer {
    vk::ResourceId albedo;
    vk::ResourceId normal;
    vk::ResourceId depth;
};

} // namespace vull
