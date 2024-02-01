#pragma once

#include <vull/maths/vec.hh>

namespace vull {

struct Vertex {
    uint16_t px, py, pz;
    uint16_t unused;
    uint32_t uv;
    uint32_t normal;
};

} // namespace vull
