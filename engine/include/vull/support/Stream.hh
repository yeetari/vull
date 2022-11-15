#pragma once

#include <vull/support/Array.hh>
#include <vull/support/Integral.hh> // IWYU pragma: keep
#include <vull/support/Result.hh>
#include <vull/support/Span.hh>
#include <vull/support/StreamError.hh>
#include <vull/support/String.hh> // IWYU pragma: keep
#include <vull/support/StringView.hh>

#include <stddef.h>
#include <stdint.h>

namespace vull {

struct Stream {
    virtual Result<void, StreamError> read(Span<void> data);
    virtual Result<void, StreamError> write(Span<const void> data);

    virtual Result<uint8_t, StreamError> read_byte();
    virtual Result<void, StreamError> write_byte(uint8_t byte);

    template <Integral T>
    Result<T, StreamError> read_be();

    template <UnsignedIntegral T>
    Result<T, StreamError> read_varint();
    template <UnsignedIntegral T>
    Result<void, StreamError> write_varint(T value);

    Result<String, StreamError> read_string();
    Result<void, StreamError> write_string(StringView string);
};

template <Integral T>
Result<T, StreamError> Stream::read_be() {
    Array<uint8_t, sizeof(T)> bytes;
    VULL_TRY(read(bytes.span()));

    T value = 0;
    for (size_t i = 0; i < sizeof(T); i++) {
        value |= static_cast<T>(bytes[i]) << (sizeof(T) - i - 1) * T(8);
    }
    return value;
}

template <UnsignedIntegral T>
Result<T, StreamError> Stream::read_varint() {
    T value = 0;
    for (T byte_count = 0; byte_count < sizeof(T); byte_count++) {
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
