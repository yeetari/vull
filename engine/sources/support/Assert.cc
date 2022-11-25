#include <vull/support/Assert.hh>

#include <vull/core/Log.hh>

#include <stdlib.h>

namespace vull {

[[noreturn]] void fatal_error(const char *error, const char *note) {
    vull::error("{}", error);
    if (note != nullptr) {
        vull::error("=> {}", note);
    }
    vull::close_log();
    abort();
}

} // namespace vull
