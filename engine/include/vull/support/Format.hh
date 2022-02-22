#pragma once

#include <vull/support/String.hh>
#include <vull/support/StringBuilder.hh>

namespace vull {

template <typename... Args>
String format(const char *fmt, Args &&...args) {
    StringBuilder builder;
    builder.append(fmt, forward<Args>(args)...);
    return builder.build();
}

} // namespace vull
