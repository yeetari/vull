#include "float_image.hh"
#include "mad_inst.hh"
#include "mad_lut.hh"

#include <vull/container/array.hh>
#include <vull/container/fixed_buffer.hh>
#include <vull/container/vector.hh>
#include <vull/core/log.hh>
#include <vull/core/tracing.hh>
#include <vull/maths/colour.hh>
#include <vull/maths/common.hh>
#include <vull/maths/vec.hh>
#include <vull/support/assert.hh>
#include <vull/support/enum.hh>
#include <vull/support/result.hh>
#include <vull/support/span.hh>
#include <vull/support/stream.hh>
#include <vull/support/string_view.hh>
#include <vull/support/utility.hh>

#include <bc5enc.hh>
#include <bc7enc.hh>
#include <string.h>

namespace vull {
namespace {

void resample_1d(const FixedBuffer<float> &source, FixedBuffer<float> &target, Vec2u source_size, uint32_t target_width,
                 uint32_t channel_count, Filter filter) {
    auto program = MadLut::instance()->lookup(source_size, target_width, filter);
    if (program.empty()) {
        vull::warn("[mad-lut] LUT miss {}x{} -> {} ({})", source_size.x(), source_size.y(), target_width,
                   vull::enum_name(filter));
        program = build_mad_program(source_size, target_width, filter);
    }

    for (uint32_t row = 0; row < source_size.y(); row++) {
        const auto row_target_offset = row * target_width * channel_count;
        const auto row_source_offset = row * source_size.x() * channel_count;
        for (const auto &inst : program) {
            const auto inst_target_offset = inst.target_index * channel_count;
            const auto inst_source_offset = inst.source_index * channel_count;
            for (uint32_t i = 0; i < channel_count; i++) {
                target[row_target_offset + inst_target_offset + i] +=
                    source[row_source_offset + inst_source_offset + i] * inst.weight;
            }
        }
    }
}

} // namespace

FloatImage FloatImage::from_colour(Colour colour) {
    FloatImage image(Vec2u(1, 1), 4);
    auto buffer = FixedBuffer<float>::create_uninitialised(4);
    buffer[0] = colour.rgba().x();
    buffer[1] = colour.rgba().y();
    buffer[2] = colour.rgba().z();
    buffer[3] = colour.rgba().w();
    image.m_mip_buffers.push(vull::move(buffer));
    return image;
}

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
            // TODO: Use encode_bc5_hq for --ultra
            Array<uint8_t, 16> compressed_block;
            rgbcx::encode_bc5(compressed_block.data(), source_block.data(), 0, 1, 2);
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
            tracing::ScopedTrace trace("Compress BC5");
            VULL_TRY(block_compress_bc5(stream, mip_buffer, mip_size));
        } else {
            tracing::ScopedTrace trace("Compress BC7");
            VULL_TRY(block_compress_bc7(stream, mip_buffer, mip_size));
        }
        mip_size = vull::max(mip_size >> 1u, Vec2u(1u));
    }
    return {};
}

void FloatImage::build_mipchain(Filter filter) {
    tracing::ScopedTrace trace("Build Mipchain");
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
    tracing::ScopedTrace trace("ColoursToVectors");
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
    tracing::ScopedTrace trace("Normalise");
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
    tracing::ScopedTrace trace("VectorsToColours");
    for (auto &mip_buffer : m_mip_buffers) {
        for (float &f : mip_buffer) {
            f = f * 0.5f + 0.5f;
        }
    }
}

} // namespace vull
