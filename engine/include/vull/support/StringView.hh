#pragma once

#include <vull/support/Span.hh>

#include <stddef.h>

namespace vull {

class StringView : public Span<const char, size_t> {
public:
    using Span::Span;
    constexpr StringView(const char *c_string) : Span(c_string, __builtin_strlen(c_string)) {}

    constexpr bool operator==(StringView other) const;
    constexpr size_t length() const { return size(); }
};

constexpr bool StringView::operator==(StringView other) const {
    if (data() == nullptr) {
        return other.data() == nullptr;
    }
    if (other.data() == nullptr) {
        return false;
    }
    if (length() != other.length()) {
        return false;
    }
    return __builtin_memcmp(data(), other.data(), length()) == 0;
}

} // namespace vull
