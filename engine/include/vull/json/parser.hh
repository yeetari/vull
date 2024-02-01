#pragma once

#include <vull/json/tree.hh> // IWYU pragma: keep
#include <vull/support/result.hh>
#include <vull/support/string_view.hh>

namespace vull::json {

// TODO: Details of parse error.
struct ParseError {};

Result<Value, ParseError> parse(StringView source);

} // namespace vull::json
