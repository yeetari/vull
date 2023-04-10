#pragma once

#include <vull/support/Result.hh>
#include <vull/support/StreamError.hh>
#include <vull/support/UniquePtr.hh>

using png_structp = struct png_struct_def *;
using png_infop = struct png_info_def *;

namespace vull {

struct Stream;

enum class PngError {
    BadSignature,
    FailedAlloc,
    Missing,
};

class PngStream {
    UniquePtr<Stream> m_stream;
    png_structp m_png;
    png_infop m_info;
    uint32_t m_width{0};
    uint32_t m_height{0};
    uint32_t m_row_byte_count{0};
    uint32_t m_pixel_byte_count{0};

    PngStream(UniquePtr<Stream> &&stream, png_structp png, png_infop info);

public:
    static Result<PngStream, PngError, StreamError> create(UniquePtr<Stream> &&stream);
    PngStream(const PngStream &) = delete;
    PngStream(PngStream &&);
    ~PngStream();

    PngStream &operator=(const PngStream &) = delete;
    PngStream &operator=(PngStream &&) = delete;

    void read_row(Span<uint8_t> row);

    Stream &stream() const { return *m_stream; }
    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }
    uint32_t row_byte_count() const { return m_row_byte_count; }
    uint32_t pixel_byte_count() const { return m_pixel_byte_count; }
};

} // namespace vull
