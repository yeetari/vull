#include <vull/support/Stream.hh>

#include <vull/support/Array.hh>
#include <vull/support/Result.hh>
#include <vull/support/Span.hh>
#include <vull/support/StreamError.hh>
#include <vull/support/String.hh>
#include <vull/support/StringView.hh>

namespace vull {

Result<size_t, StreamError> Stream::seek(StreamOffset, SeekMode) {
    return StreamError::NotImplemented;
}

Result<size_t, StreamError> Stream::read(Span<void>) {
    return StreamError::NotImplemented;
}

Result<void, StreamError> Stream::write(Span<const void>) {
    return StreamError::NotImplemented;
}

Result<uint8_t, StreamError> Stream::read_byte() {
    uint8_t byte;
    if (VULL_TRY(read({&byte, 1})) != 1) {
        return StreamError::Truncated;
    }
    return byte;
}

Result<void, StreamError> Stream::write_byte(uint8_t byte) {
    VULL_TRY(write({&byte, 1}));
    return {};
}

// TODO: Inconsistent Span sizing, maybe Span can be reworked to default to size_t and allow implicit size type
//       upcasting.
Result<String, StreamError> Stream::read_string() {
    const auto length = VULL_TRY(read_varint<uint32_t>());
    String value(length);
    if (VULL_TRY(read({value.data(), length})) != length) {
        return StreamError::Truncated;
    }
    return value;
}

Result<void, StreamError> Stream::write_string(StringView string) {
    VULL_TRY(write_varint(string.length()));
    VULL_TRY(write(string.as<const void, uint32_t>()));
    return {};
}

Result<void, StreamError> Stream::write_c_string(StringView string) {
    VULL_TRY(write(string.as<const void, uint32_t>()));
    return {};
}

} // namespace vull
