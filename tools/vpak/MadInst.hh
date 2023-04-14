#pragma once

#include <vull/container/Vector.hh>
#include <vull/maths/Vec.hh>

#include <stdint.h>

namespace vull {

enum class Filter {
    Box,
    Gaussian,
};

struct MadInst {
    uint32_t target_index;
    uint32_t source_index;
    float weight;
};

Vector<MadInst> build_mad_program(Vec2u source_size, uint32_t target_width, Filter filter);

} // namespace vull
