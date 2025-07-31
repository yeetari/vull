#pragma once

#include <vull/maths/vec.hh>
#include <vull/vulkan/render_graph_defs.hh>

namespace vull {

struct GBuffer {
    Vec2u viewport_extent;
    vk::ResourceId albedo;
    vk::ResourceId normal;
    vk::ResourceId depth;
};

} // namespace vull
