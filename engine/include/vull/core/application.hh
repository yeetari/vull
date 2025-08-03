#pragma once

#include <vull/support/function.hh> // IWYU pragma: keep

namespace vull {

class ArgsParser;

int start_application(int argc, char **argv, ArgsParser &args_parser, Function<void()> start_fn,
                      Function<void()> stop_fn);

} // namespace vull
