#pragma once

#include <vull/support/string_view.hh>

namespace vull {

class SourceLocation {
    StringView m_file_name;
    StringView m_function_name;
    unsigned m_line{0};

public:
    static consteval SourceLocation current(const char *file = __builtin_FILE(),
                                            const char *function = __builtin_FUNCTION(),
                                            unsigned line = __builtin_LINE()) {
        return {file, function, line};
    }

    constexpr SourceLocation() = default;
    constexpr SourceLocation(StringView file_name, StringView function_name, unsigned line)
        : m_file_name(file_name), m_function_name(function_name), m_line(line) {}

    constexpr StringView file_name() const { return m_file_name; }
    constexpr StringView function_name() const { return m_function_name; }
    constexpr unsigned line() const { return m_line; }
};

} // namespace vull
