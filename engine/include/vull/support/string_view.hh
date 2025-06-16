#pragma once

#include <vull/maths/common.hh>
#include <vull/support/assert.hh>
#include <vull/support/integral.hh>
#include <vull/support/optional.hh>
#include <vull/support/span.hh>
#include <vull/support/utility.hh>

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

    template <Integral T>
    constexpr Optional<T> to_integral() const;

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

template <Integral T>
constexpr Optional<T> StringView::to_integral() const {
    const auto *it = begin();
    if (it == end()) {
        return vull::nullopt;
    }

    // Parse sign.
    T sign = T(1);
    if constexpr (vull::is_signed<T>) {
        if (*it == '+' || *it == '-') {
            if (*it == '-') {
                sign = T(-1);
            }
            it++;
        }
    }

    // Reject a + or - on its own.
    if (it == end()) {
        return vull::nullopt;
    }

    T value = T(0);
    while (it != end()) {
        const char ch = *it++;
        if (ch < '0' || ch > '9') {
            return vull::nullopt;
        }
        if (__builtin_mul_overflow(value, T(10), &value)) {
            return vull::nullopt;
        }
        if (__builtin_add_overflow(value, sign * T(ch - '0'), &value)) {
            return vull::nullopt;
        }
    }
    return value;
}

constexpr bool operator==(StringView lhs, type_identity<StringView> rhs) {
    return lhs.length() == rhs.length() && lhs.compare(rhs) == 0;
}

} // namespace vull
