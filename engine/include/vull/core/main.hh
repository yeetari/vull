#pragma once

#include <vull/container/vector.hh>
#include <vull/support/string_view.hh>

namespace vull {

class ArgsParser;

void add_engine_args(ArgsParser &args_parser);

} // namespace vull

void vull_main(vull::Vector<vull::StringView> &&args);
