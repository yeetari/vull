#pragma once

#include <vull/vulkan/render_graph_defs.hh>

namespace vull {

struct GBuffer {
    vk::ResourceId albedo;
    vk::ResourceId normal;
    vk::ResourceId depth;
};

} // namespace vull
