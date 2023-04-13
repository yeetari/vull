#include "MadLut.hh"

#include <vull/core/Log.hh>
#include <vull/maths/Common.hh>
#include <vull/maths/Vec.hh>
#include <vull/support/Array.hh>
#include <vull/support/Enum.hh>
#include <vull/support/FixedBuffer.hh>
#include <vull/support/Result.hh>
#include <vull/support/SpanStream.hh>
#include <vull/support/Stream.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>

namespace vull {
namespace {
#include "MadLut.in"
} // namespace

SpanStream MadLut::lookup(uint32_t offset) {
    SpanStream stream({m_buffer.data(), static_cast<uint32_t>(m_buffer.size())});
    VULL_ASSUME(stream.seek(offset, SeekMode::Set));
    return stream;
}

Vector<MadInst> MadLut::lookup(Vec2u source_size, uint32_t target_width, Filter filter) {
    const auto log_width = vull::log2(source_size.x());
    if (log_width >= s_gaussian_offset_lut.size()) {
        return {};
    }
    const auto log_height = vull::log2(source_size.y());
    if (log_height >= s_gaussian_offset_lut[log_width].size()) {
        return {};
    }
    const auto log_target_width = vull::log2(target_width);
    if (log_target_width >= s_gaussian_offset_lut[log_width][log_height].size()) {
        return {};
    }

    auto &table = filter == Filter::Gaussian ? s_gaussian_offset_lut : s_box_offset_lut;
    auto offset = table[log_width][log_height][log_target_width];
    if (offset == -1) {
        vull::warn("[mad-lut] Invalid combination {}x{} -> {} ({})", source_size.x(), source_size.y(), target_width,
                   vull::enum_name(filter));
        return {};
    }

    auto stream = lookup(static_cast<uint32_t>(offset));
    auto count = VULL_EXPECT(stream.read_be<uint32_t>());
    Vector<MadInst> program;
    program.ensure_capacity(count);
    for (uint32_t i = 0; i < count; i++) {
        auto target_index = VULL_EXPECT(stream.read_be<uint32_t>());
        auto source_index = VULL_EXPECT(stream.read_be<uint32_t>());
        float weight;
        VULL_EXPECT(stream.read({&weight, sizeof(float)}));
        program.push({target_index, source_index, weight});
    }
    return program;
}

} // namespace vull
