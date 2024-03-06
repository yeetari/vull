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

    constexpr int compare(StringView other) const;
    constexpr StringView substr(size_t offset) const;
    constexpr StringView substr(size_t offset, size_t length) const;

    constexpr bool starts_with(char ch) const;
    constexpr bool ends_with(char ch) const;

    constexpr bool starts_with(StringView other) const;
    constexpr bool ends_with(StringView other) const;

    constexpr bool operator==(StringView other) const;
    constexpr size_t length() const { return size(); }
};

constexpr int StringView::compare(StringView other) const {
    int ret = __builtin_memcmp(data(), other.data(), vull::min(length(), other.length()));
    if (ret == 0) {
        if (length() < other.length()) {
            ret = -1;
        } else if (length() > other.length()) {
            ret = 1;
        }
    }
    return ret;
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
