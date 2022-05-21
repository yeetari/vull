#pragma once

#include <vull/support/Array.hh>
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

    template <typename T>
    void append_single(T arg,
                       const char *opts) requires(IsSame<T, uint8_t> || IsSame<T, uint16_t> || IsSame<T, uint32_t>) {
        append_single(static_cast<size_t>(arg), opts);
    }

    template <typename T>
    void append_part(const char *&fmt, const T &arg);

public:
    template <typename... Args>
    void append(const char *fmt, const Args &...args);

    String build();
    String build_copy() const;
    size_t length() const { return m_buffer.size(); }
};

template <typename T>
void StringBuilder::append_part(const char *&fmt, const T &arg) {
    while (*fmt != '\0' && *fmt != '{') {
        m_buffer.push(*fmt++);
    }
    if (*fmt++ != '{') {
        return;
    }
    Array<char, 4> opts{};
    for (uint32_t i = 0; i < opts.size() && *fmt != '}';) {
        opts[i++] = *fmt++;
    }
    append_single(arg, opts.data());
    fmt++;
}

template <typename... Args>
void StringBuilder::append(const char *fmt, const Args &...args) {
    (append_part(fmt, args), ...);
    // TODO: If we knew the remaining size of the format string (if we took in a StringView for fmt), this code could be
    //       much faster.
    while (*fmt != '\0') {
        m_buffer.push(*fmt++);
    }
}

} // namespace vull
