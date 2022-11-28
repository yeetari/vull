#pragma once

#include <vull/support/Array.hh>
#include <vull/support/Span.hh>
#include <vull/support/String.hh>
#include <vull/support/StringView.hh>
#include <vull/support/Vector.hh>

#include <stddef.h>
#include <stdint.h>

// TODO: Testing and benchmarking.
// TODO: Compile time format strings + checking?

namespace vull {

class StringBuilder {
    LargeVector<char> m_buffer;

    void append_single(float, const char *);
    void append_single(size_t, const char *);
    void append_single(StringView, const char *);

    template <Integral T>
    void append_single(T arg, const char *opts) {
        append_single(static_cast<size_t>(arg), opts);
    }

    template <typename T>
    void append_part(StringView fmt, size_t &index, const T &arg);

public:
    template <typename... Args>
    void append(StringView fmt, const Args &...args);
    void append(char ch);

    String build();
    String build_copy() const;
    size_t length() const { return m_buffer.size(); }
};

template <typename T>
void StringBuilder::append_part(StringView fmt, size_t &index, const T &arg) {
    while (index < fmt.length() && fmt[index] != '{') {
        m_buffer.push(fmt[index++]);
    }

    if (index >= fmt.length() || fmt[index++] != '{') {
        return;
    }

    Array<char, 4> opts{};
    for (uint32_t i = 0; i < opts.size() && fmt[index] != '}';) {
        opts[i++] = fmt[index++];
    }
    // NOLINTNEXTLINE
    append_single(arg, opts.data());
    index++;
}

template <typename... Args>
void StringBuilder::append(StringView fmt, const Args &...args) {
    size_t index = 0;
    (append_part(fmt, index, args), ...);
    m_buffer.extend(LargeSpan<const char>{fmt.begin() + index, fmt.length() - index});
}

} // namespace vull
