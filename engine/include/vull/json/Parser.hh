#pragma once

#include <vull/json/Tree.hh> // IWYU pragma: keep
#include <vull/support/Result.hh>
#include <vull/support/StringView.hh>

namespace vull::json {

// TODO: Details of parse error.
struct ParseError {};

Result<Value, ParseError> parse(StringView source);

} // namespace vull::json
