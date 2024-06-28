#pragma once

#include <vull/maths/common.hh>
#include <vull/support/assert.hh>
#include <vull/support/span.hh>

#include <stddef.h>

namespace vull {

class StringView : public Span<const char> {
public:
    using Span::Span;
    constexpr StringView(const char *c_string) : Span(c_string, __builtin_strlen(c_string)) {}
    StringView(nullptr_t) = delete;

    constexpr int compare(StringView other) const;
    constexpr StringView substr(size_t offset) const;
    constexpr StringView substr(size_t offset, size_t length) const;

    constexpr bool starts_with(char ch) const;
    constexpr bool ends_with(char ch) const;

    constexpr bool starts_with(StringView other) const;
    constexpr bool ends_with(StringView other) const;

    constexpr size_t length() const { return size(); }
};

constexpr int StringView::compare(StringView other) const {
    size_t common_length = vull::min(length(), other.length());
    for (size_t i = 0; i < common_length; i++) {
        if (data()[i] < other.data()[i]) {
            return -1;
        }
        if (other.data()[i] < data()[i]) {
            return 1;
        }
    }

    // Else equal up to their common length, compare lengths.
    return length() == other.length() ? 0 : (length() < other.length() ? -1 : 1);
}

constexpr StringView StringView::substr(size_t offset) const {
    return substr(offset, length() - offset);
}

constexpr StringView StringView::substr(size_t offset, size_t length) const {
    VULL_ASSERT(offset + length <= size());
    return {data() + offset, length};
}

constexpr bool StringView::starts_with(char ch) const {
    return !empty() && begin()[0] == ch;
}

constexpr bool StringView::ends_with(char ch) const {
    return !empty() && end()[-1] == ch;
}

constexpr bool StringView::starts_with(StringView other) const {
    return length() >= other.length() && substr(0, other.length()).compare(other) == 0;
}

constexpr bool StringView::ends_with(StringView other) const {
    return length() >= other.length() && substr(length() - other.length()).compare(other) == 0;
}

constexpr bool operator==(StringView lhs, type_identity<StringView> rhs) {
    return lhs.length() == rhs.length() && lhs.compare(rhs) == 0;
}

} // namespace vull
