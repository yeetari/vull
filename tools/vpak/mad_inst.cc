#include "mad_inst.hh"

#include <vull/container/vector.hh>
#include <vull/maths/common.hh>
#include <vull/maths/vec.hh>
#include <vull/support/utility.hh>

namespace vull {
namespace {

float box_filter(float t) {
    return t <= 0.5f ? 1.0f : 0.0f;
}

float gaussian_filter(float t) {
    if (t >= 2.0f) {
        return 0.0f;
    }
    const float scale = 1.0f / vull::sqrt(vull::half_pi<float>);
    return vull::exp(-2.0f * t * t) * scale;
}

} // namespace

Vector<MadInst> build_mad_program(Vec2u source_size, uint32_t target_width, Filter filter) {
    Vector<MadInst> program;

    auto filter_bounds = static_cast<float>(target_width);
    if (filter == Filter::Gaussian) {
        filter_bounds *= 2.0f;
    }

    const float dtarget = 1.0f / static_cast<float>(target_width);
    float xtarget = dtarget / 2.0f;
    for (uint32_t itarget = 0; itarget < target_width; itarget++, xtarget += dtarget) {
        uint32_t count = 0;
        float sum = 0.0f;

        const auto isource_lower = int32_t((xtarget - filter_bounds) * static_cast<float>(source_size.x()));
        const auto isource_upper = int32_t(vull::ceil((xtarget + filter_bounds) * static_cast<float>(source_size.x())));
        for (int32_t isource = isource_lower; isource <= isource_upper; isource++) {
            const float xsource = (static_cast<float>(isource) + 0.5f) / static_cast<float>(source_size.x());
            const bool outside_image = isource < 0 || isource >= static_cast<int32_t>(source_size.x());
            const bool outside_range = xsource < 0.0f || xsource >= 1.0f;
            if (outside_image || outside_range) {
                continue;
            }
            const float t = static_cast<float>(target_width) * vull::abs(xsource - xtarget);
            const float weight = filter == Filter::Gaussian ? gaussian_filter(t) : box_filter(t);
            if (weight != 0.0f) {
                program.push({itarget, static_cast<uint32_t>(isource), weight});
                count++;
                sum += weight;
            }
        }

        if (sum != 0.0f) {
            for (uint32_t i = 0; i < count; i++) {
                program[program.size() - count + i].weight /= sum;
            }
        }
    }
    return program;
}

} // namespace vull
