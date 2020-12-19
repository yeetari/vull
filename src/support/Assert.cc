#include <support/Assert.hh>

#include <fmt/color.h>
#include <fmt/core.h>

#include <cstdlib>

[[noreturn]] void assertion_failed(const char *file, unsigned int line, const char *expr) {
    fmt::print(fmt::fg(fmt::color::orange_red), "Assertion '{}' failed at {}:{}\n", expr, file, line);
    std::abort();
}
