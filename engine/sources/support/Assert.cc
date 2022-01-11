#include <vull/support/Assert.hh>

#include <stdio.h>
#include <stdlib.h>

namespace vull {

[[noreturn]] void fatal_error(const char *error, const char *note) {
    fprintf(stderr, "%s\n", error);
    if (note != nullptr) {
        fprintf(stderr, "=> %s\n", note);
    }
    abort();
}

} // namespace vull
