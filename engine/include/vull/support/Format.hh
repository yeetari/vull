#pragma once

#include <vull/support/String.hh>
#include <vull/support/StringBuilder.hh>
#include <vull/support/StringView.hh>

namespace vull {

template <typename... Args>
String format(StringView fmt, Args &&...args) {
    StringBuilder builder;
    builder.append(fmt, forward<Args>(args)...);
    return builder.build();
}

} // namespace vull
