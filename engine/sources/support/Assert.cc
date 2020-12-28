#include <vull/support/Assert.hh>

#include <vull/support/Log.hh>

#include <cstdlib>

[[noreturn]] void assertion_failed(const char *file, unsigned int line, const char *expr) {
    Log::error("core", "Assertion '%s' failed at %s:%d", expr, file, line);
    std::abort();
}
