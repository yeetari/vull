#include "FloatImage.hh"

#include <vull/maths/Common.hh>
#include <vull/maths/Vec.hh>
#include <vull/support/Array.hh>
#include <vull/support/Assert.hh>
#include <vull/support/FixedBuffer.hh>
#include <vull/support/Result.hh>
#include <vull/support/Span.hh>
#include <vull/support/Stream.hh>
#include <vull/support/StreamError.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>

#include <bc7enc.hh>
#include <rgbcx.hh>
#include <string.h>

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

void resample_1d(const FixedBuffer<float> &source, FixedBuffer<float> &target, Vec2u source_size, uint32_t target_width,
                 uint32_t channel_count, Filter filter) {
    struct MadInst {
        uint32_t target_index;
        uint32_t source_index;
        float weight;
    };
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

    auto old_program = vull::move(program);
    program.ensure_capacity(old_program.size() * channel_count);
    for (auto mad : old_program) {
        mad.source_index *= channel_count;
        mad.target_index *= channel_count;
        for (uint32_t i = 0; i < channel_count; i++) {
            program.push(mad);
            mad.source_index++;
            mad.target_index++;
        }
    }

    const auto *source_row = source.data();
    auto *target_row = target.data();
    for (uint32_t row = 0; row < source_size.y(); row++) {
        for (auto mad : program) {
            target_row[mad.target_index] += source_row[mad.source_index] * mad.weight;
        }
        target_row += target_width * channel_count;
        source_row += source_size.x() * channel_count;
    }
}

} // namespace

FloatImage FloatImage::from_unorm(Span<const uint8_t> bitmap, Vec2u size, uint32_t channel_count) {
    VULL_ASSERT(bitmap.size() % channel_count == 0);
    FloatImage image(size, channel_count);
    auto buffer = FixedBuffer<float>::create_uninitialised(size.x() * size.y() * channel_count);
    for (uint32_t i = 0; i < bitmap.size(); i++) {
        buffer[i] = float(bitmap[i]) / 255.0f;
    }
    image.m_mip_buffers.push(vull::move(buffer));
    return image;
}

Result<void, StreamError> FloatImage::block_compress_bc5(Stream &stream, FixedBuffer<float> &buffer, Vec2u size) const {
    for (uint32_t block_y = 0; block_y < size.y(); block_y += 4) {
        for (uint32_t block_x = 0; block_x < size.x(); block_x += 4) {
            // 32-byte (4x4 * 2 bytes per pixel) input.
            Array<uint8_t, 32> source_block{};

            // Extract block.
            for (uint32_t y = 0; y < 4 && block_y + y < size.y(); y++) {
                const auto *row_data = &buffer[(block_y + y) * size.x() * m_channel_count];
                for (uint32_t x = 0; x < 4 && block_x + x < size.x(); x++) {
                    const auto *pixel = &row_data[(block_x + x) * m_channel_count];
                    source_block[y * 8 + x * 2] = uint8_t(pixel[0] * 255.0f);
                    source_block[y * 8 + x * 2 + 1] = uint8_t(pixel[1] * 255.0f);
                }
            }

            // 128-bit compressed block.
            Array<uint8_t, 16> compressed_block;
            rgbcx::encode_bc5_hq(compressed_block.data(), source_block.data(), 0, 1, 2);
            VULL_TRY(stream.write(compressed_block.span()));
        }
    }
    return {};
}

Result<void, StreamError> FloatImage::block_compress_bc7(Stream &stream, FixedBuffer<float> &buffer, Vec2u size) const {
    bc7enc_compress_block_params params{};
    bc7enc_compress_block_params_init(&params);

    for (uint32_t block_y = 0; block_y < size.y(); block_y += 4) {
        for (uint32_t block_x = 0; block_x < size.x(); block_x += 4) {
            // 64-byte (4x4 * 4 bytes per pixel) input.
            Array<uint8_t, 64> source_block{};

            // Extract block.
            for (uint32_t y = 0; y < 4 && block_y + y < size.y(); y++) {
                const auto *row_data = &buffer[(block_y + y) * size.x() * m_channel_count];
                for (uint32_t x = 0; x < 4 && block_x + x < size.x(); x++) {
                    const auto *pixel = &row_data[(block_x + x) * m_channel_count];
                    source_block[y * 16 + x * 4] = uint8_t(pixel[0] * 255.0f);
                    source_block[y * 16 + x * 4 + 1] = uint8_t(pixel[1] * 255.0f);
                    source_block[y * 16 + x * 4 + 2] = uint8_t(pixel[2] * 255.0f);
                    if (m_channel_count == 4) {
                        source_block[y * 16 + x * 4 + 3] = uint8_t(pixel[3] * 255.0f);
                    } else {
                        source_block[y * 16 + x * 4 + 3] = 255;
                    }
                }
            }

            // 128-bit compressed block.
            Array<uint8_t, 16> compressed_block;
            bc7enc_compress_block(compressed_block.data(), source_block.data(), &params);
            VULL_TRY(stream.write(compressed_block.span()));
        }
    }
    return {};
}

Result<void, StreamError> FloatImage::block_compress(Stream &stream, bool bc5) {
    auto mip_size = m_size;
    for (auto &mip_buffer : m_mip_buffers) {
        if (bc5) {
            VULL_TRY(block_compress_bc5(stream, mip_buffer, mip_size));
        } else {
            VULL_TRY(block_compress_bc7(stream, mip_buffer, mip_size));
        }
        mip_size = vull::max(mip_size >> 1u, Vec2u(1u));
    }
    return {};
}

void FloatImage::build_mipchain(Filter filter) {
    const auto mip_count = vull::log2(vull::max(m_size.x(), m_size.y())) + 1u;
    m_mip_buffers.ensure_size(mip_count);

    auto mip_size = m_size;
    for (uint32_t mip_level = 1; mip_level < mip_count; mip_level++) {
        auto &buffer = m_mip_buffers[mip_level];
        mip_size = vull::max(mip_size >> 1u, Vec2u(1u));

        buffer = FixedBuffer<float>::create_zeroed(mip_size.x() * m_size.y() * m_channel_count);
        resample_1d(m_mip_buffers.first(), buffer, m_size, mip_size.x(), m_channel_count, filter);

        auto new_buffer = FixedBuffer<float>::create_uninitialised(mip_size.x() * m_size.y() * m_channel_count);
        for (uint32_t n = 0; n < mip_size.x() * m_size.y(); ++n) {
            const uint32_t i = n / mip_size.x();
            const uint32_t j = n % mip_size.x();
            float const *src = buffer.data() + m_channel_count * n;
            float *dst = new_buffer.data() + m_channel_count * (m_size.y() * j + i);
            memcpy(dst, src, m_channel_count * sizeof(float));
        }

        buffer = FixedBuffer<float>::create_zeroed(mip_size.y() * mip_size.x() * m_channel_count);
        resample_1d(new_buffer, buffer, {m_size.y(), mip_size.x()}, mip_size.y(), m_channel_count, filter);

        new_buffer = FixedBuffer<float>::create_uninitialised(mip_size.y() * mip_size.x() * m_channel_count);
        for (uint32_t n = 0; n < mip_size.y() * mip_size.x(); ++n) {
            const uint32_t i = n / mip_size.y();
            const uint32_t j = n % mip_size.y();
            float const *src = buffer.data() + m_channel_count * n;
            float *dst = new_buffer.data() + m_channel_count * (mip_size.x() * j + i);
            memcpy(dst, src, m_channel_count * sizeof(float));
        }
        buffer = vull::move(new_buffer);
    }
}

void FloatImage::colours_to_vectors() {
    for (auto &mip_buffer : m_mip_buffers) {
        for (float &f : mip_buffer) {
            f = f * 2.0f - 1.0f;
        }
    }
}

void FloatImage::drop_mips(uint32_t count) {
    VULL_ASSERT(count < m_mip_buffers.size() - 1);
    for (uint32_t i = 0; i < m_mip_buffers.size() - count; i++) {
        m_mip_buffers[i] = vull::move(m_mip_buffers[i + count]);
    }
    for (uint32_t i = 0; i < count; i++) {
        m_mip_buffers.pop();
    }
    m_size >>= count;
}

template <typename VecType>
static void normalise_fn(FixedBuffer<float> &buffer) {
    auto *pixel_data = reinterpret_cast<VecType *>(buffer.data());
    for (size_t i = 0; i < buffer.size_bytes() / sizeof(VecType); i++) {
        pixel_data[i] = vull::normalise(pixel_data[i]);
    }
}

void FloatImage::normalise() {
    for (auto &mip_buffer : m_mip_buffers) {
        switch (m_channel_count) {
        case 2:
            normalise_fn<Vec2f>(mip_buffer);
            break;
        case 3:
            normalise_fn<Vec3f>(mip_buffer);
            break;
        case 4:
            normalise_fn<Vec4f>(mip_buffer);
            break;
        }
    }
}

void FloatImage::vectors_to_colours() {
    for (auto &mip_buffer : m_mip_buffers) {
        for (float &f : mip_buffer) {
            f = f * 0.5f + 0.5f;
        }
    }
}

} // namespace vull
