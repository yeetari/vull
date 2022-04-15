#pragma once

#include <vull/support/Span.hh>

#include <stddef.h>

namespace vull {

class StringView : public Span<const char, size_t> {
public:
    using Span::Span;
    constexpr StringView(const char *c_string) : Span(c_string, __builtin_strlen(c_string)) {}

    bool operator==(StringView other) const;

    constexpr bool empty() const { return length() == 0; }
    constexpr size_t length() const { return size(); }
};

} // namespace vull
