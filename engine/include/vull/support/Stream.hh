#pragma once

#include <vull/container/Array.hh>
#include <vull/support/Integral.hh> // IWYU pragma: keep
#include <vull/support/Integral.hh>
#include <vull/support/Result.hh>
#include <vull/support/Span.hh>
#include <vull/support/StreamError.hh>
#include <vull/support/String.hh> // IWYU pragma: keep
#include <vull/support/StringView.hh>
#include <vull/support/UniquePtr.hh>

#include <stdint.h>
#include <sys/types.h>

namespace vull {

class StreamOffset {
    ssize_t m_value;

public:
    template <SignedIntegral T>
    StreamOffset(T value) : m_value(value) {}
    template <UnsignedIntegral T>
    StreamOffset(T value) : m_value(static_cast<ssize_t>(value)) {}

    operator ssize_t() const { return m_value; }
};

enum class SeekMode {
    Set,
    Add,
    End,
};

struct Stream {
    Stream() = default;
    Stream(const Stream &) = default;
    Stream(Stream &&) = default;
    virtual ~Stream() = default;

    Stream &operator=(const Stream &) = default;
    Stream &operator=(Stream &&) = default;

    virtual UniquePtr<Stream> clone_unique() const { return {}; }
    virtual Result<size_t, StreamError> seek(StreamOffset offset, SeekMode mode);
    virtual Result<size_t, StreamError> read(Span<void> data);
    virtual Result<void, StreamError> write(Span<const void> data);

    virtual Result<uint8_t, StreamError> read_byte();
    virtual Result<void, StreamError> write_byte(uint8_t byte);

    template <Integral T>
    Result<T, StreamError> read_be();
    template <Integral T>
    Result<void, StreamError> write_be(T value);

    template <UnsignedIntegral T>
    Result<T, StreamError> read_varint();
    template <UnsignedIntegral T>
    Result<void, StreamError> write_varint(T value);

    Result<String, StreamError> read_string();
    Result<void, StreamError> write_string(StringView string);
    Result<void, StreamError> write_c_string(StringView string);
};

template <Integral T>
Result<T, StreamError> Stream::read_be() {
    Array<uint8_t, sizeof(T)> bytes;
    if (VULL_TRY(read(bytes.span())) != sizeof(T)) {
        return StreamError::Truncated;
    }

    T value = 0;
    for (uint32_t i = 0; i < sizeof(T); i++) {
        const auto shift = (sizeof(T) - i - 1) * T(8);
        value |= static_cast<T>(bytes[i]) << shift;
    }
    return value;
}

template <Integral T>
Result<void, StreamError> Stream::write_be(T value) {
    Array<uint8_t, sizeof(T)> bytes;
    for (uint32_t i = 0; i < sizeof(T); i++) {
        const auto shift = (sizeof(T) - i - 1) * T(8);
        bytes[i] = static_cast<uint8_t>((value >> shift) & 0xffu);
    }
    VULL_TRY(write(bytes.span()));
    return {};
}

template <UnsignedIntegral T>
Result<T, StreamError> Stream::read_varint() {
    T value = 0;
    for (T byte_count = 0; byte_count <= sizeof(T); byte_count++) {
        uint8_t byte = VULL_TRY(read_byte());
        value |= static_cast<T>(byte & 0x7fu) << (byte_count * 7u);
        if ((byte & 0x80u) == 0u) {
            break;
        }
    }
    return value;
}

template <UnsignedIntegral T>
Result<void, StreamError> Stream::write_varint(T value) {
    while (value >= 128) {
        VULL_TRY(write_byte((value & 0x7fu) | 0x80u));
        value >>= 7u;
    }
    VULL_TRY(write_byte(value & 0x7fu));
    return {};
}

} // namespace vull
