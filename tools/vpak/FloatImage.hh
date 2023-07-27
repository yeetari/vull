#pragma once

#include <vull/container/FixedBuffer.hh>
#include <vull/container/Vector.hh>
#include <vull/maths/Colour.hh> // IWYU pragma: keep
#include <vull/maths/Vec.hh>
#include <vull/support/Result.hh>
#include <vull/support/Span.hh>

#include <stdint.h>

namespace vull {

enum class Filter;
enum class StreamError;
struct Stream;

class FloatImage {
    Vec2u m_size{};
    uint32_t m_channel_count{};
    Vector<FixedBuffer<float>> m_mip_buffers;

    FloatImage(Vec2u size, uint32_t channel_count) : m_size(size), m_channel_count(channel_count) {}

    Result<void, StreamError> block_compress_bc5(Stream &, FixedBuffer<float> &, Vec2u) const;
    Result<void, StreamError> block_compress_bc7(Stream &, FixedBuffer<float> &, Vec2u) const;

public:
    static FloatImage from_colour(Colour colour);
    static FloatImage from_unorm(Span<const uint8_t> bitmap, Vec2u size, uint32_t channel_count);

    FloatImage() = default;

    Result<void, StreamError> block_compress(Stream &stream, bool bc5);
    void build_mipchain(Filter filter);
    void colours_to_vectors();
    void drop_mips(uint32_t count);
    void normalise();
    void vectors_to_colours();

    Vec2u size() const { return m_size; }
    uint32_t channel_count() const { return m_channel_count; }
    uint32_t mip_count() const { return m_mip_buffers.size(); }
};

} // namespace vull
