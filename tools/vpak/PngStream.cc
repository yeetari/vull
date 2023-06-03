#include "PngStream.hh"

#include <vull/support/Stream.hh>

#include <new>
#ifdef BUILD_PNG
#include <png.h>
#endif

// TODO: Proper error handling from libpng.

namespace vull {

#ifdef BUILD_PNG

Result<PngStream, PngError, StreamError> PngStream::create(UniquePtr<Stream> &&stream) {
    Array<uint8_t, 8> signature{};
    VULL_TRY(stream->read(signature.span()));
    if (png_sig_cmp(signature.data(), 0, 8) != 0) {
        return PngError::BadSignature;
    }

    auto *png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (png == nullptr) {
        return PngError::FailedAlloc;
    }

    auto *info = png_create_info_struct(png);
    if (info == nullptr) {
        png_destroy_read_struct(&png, nullptr, nullptr);
        return PngError::FailedAlloc;
    }
    png_set_sig_bytes(png, 8);
    return PngStream(vull::move(stream), png, info);
}

static void read_fn(png_structp png, png_bytep out_data, size_t size) {
    // TODO: Forward errors.
    auto &stream = *static_cast<PngStream *>(png_get_io_ptr(png));
    VULL_EXPECT(stream.stream().read({out_data, size}));
}

PngStream::PngStream(UniquePtr<Stream> &&stream, png_structp png, png_infop info)
    : m_stream(vull::move(stream)), m_png(png), m_info(info) {
    png_set_read_fn(png, this, read_fn);
    png_read_info(png, info);
    m_width = png_get_image_width(png, info);
    m_height = png_get_image_height(png, info);
    m_row_byte_count = static_cast<uint32_t>(png_get_rowbytes(png, info));
    m_pixel_byte_count = m_row_byte_count / m_width;
}

PngStream::PngStream(PngStream &&other) {
    m_stream = vull::move(other.m_stream);
    m_png = vull::exchange(other.m_png, nullptr);
    m_info = vull::exchange(other.m_info, nullptr);
    m_width = vull::exchange(other.m_width, 0u);
    m_height = vull::exchange(other.m_height, 0u);
    m_row_byte_count = vull::exchange(other.m_row_byte_count, 0u);
    m_pixel_byte_count = vull::exchange(other.m_pixel_byte_count, 0u);
    if (m_png != nullptr) {
        png_set_read_fn(m_png, this, read_fn);
    }
}

PngStream::~PngStream() {
    png_destroy_info_struct(m_png, &m_info);
    png_destroy_read_struct(&m_png, nullptr, nullptr);
}

void PngStream::read_row(Span<uint8_t> row) {
    VULL_ASSERT(row.size() >= m_row_byte_count);
    png_read_row(m_png, row.data(), nullptr);
}

#else

Result<PngStream, PngError, StreamError> PngStream::create(UniquePtr<Stream> &&) {
    return PngError::Missing;
}

PngStream::PngStream(UniquePtr<Stream> &&, png_structp, png_infop) {
    VULL_ENSURE_NOT_REACHED();
}

PngStream::PngStream(PngStream &&) {
    VULL_ENSURE_NOT_REACHED();
}

PngStream::~PngStream() = default;

void PngStream::read_row(Span<uint8_t>) {
    VULL_ENSURE_NOT_REACHED();
}

#endif

} // namespace vull
