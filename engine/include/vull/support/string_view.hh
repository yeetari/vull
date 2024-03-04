#pragma once

#include <vull/support/assert.hh>
#include <vull/support/span.hh>

#include <stddef.h>

namespace vull {

class StringView : public Span<const char> {
public:
    using Span::Span;
    constexpr StringView(const char *c_string) : Span(c_string, __builtin_strlen(c_string)) {}

    constexpr StringView substr(size_t offset) const;
    constexpr StringView substr(size_t offset, size_t length) const;
    constexpr bool operator==(StringView other) const;
    constexpr size_t length() const { return size(); }
};

constexpr StringView StringView::substr(size_t offset) const {
    return substr(offset, length() - offset);
}

constexpr StringView StringView::substr(size_t offset, size_t length) const {
    VULL_ASSERT(offset + length <= size());
    return {data() + offset, length};
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
