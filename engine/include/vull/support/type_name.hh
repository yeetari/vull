#pragma once

#include <vull/support/string_view.hh>

namespace vull {

template <typename T>
consteval StringView type_name() {
    constexpr StringView name(__PRETTY_FUNCTION__);
    for (size_t i = 0; i < name.length() - 3; i++) {
        if (name[i] == 'T' && name[i + 1] == ' ' && name[i + 2] == '=') {
            return {name.data() + i + 4, name.length() - i - 5};
        }
    }
    return "<unknown>";
}

} // namespace vull
