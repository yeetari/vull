#pragma once

#include <vull/support/String.hh>
#include <vull/support/StringView.hh>
#include <vull/support/Vector.hh>

#include <stddef.h>

// TODO: Testing and benchmarking.
// TODO: Compile time format strings + checking?

namespace vull {

class StringBuilder {
    LargeVector<char> m_buffer;

    void append_single(float arg);
    void append_single(size_t arg);
    void append_single(StringView arg);

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
    if (*fmt++ != '{' || *fmt++ != '}') {
        return;
    }
    append_single(arg);
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
