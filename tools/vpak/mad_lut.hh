#pragma once

#include <vull/container/fixed_buffer.hh>
#include <vull/container/vector.hh>
#include <vull/maths/vec.hh>
#include <vull/support/span_stream.hh>
#include <vull/support/utility.hh>

#include <stdint.h>

namespace vull {

enum class Filter;
struct MadInst;

class MadLut {
    FixedBuffer<uint8_t> m_buffer;

private:
    static inline MadLut *s_instance = nullptr;
    SpanStream lookup(uint32_t offset);

public:
    static void set_instance(MadLut *instance) { s_instance = instance; }
    static MadLut *instance() { return s_instance; }

    explicit MadLut(FixedBuffer<uint8_t> &&buffer) : m_buffer(vull::move(buffer)) {}
    Vector<MadInst> lookup(Vec2u source_size, uint32_t texture_width, Filter filter);
};

} // namespace vull
