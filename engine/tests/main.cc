#include <vull/support/Test.hh>

#include <stdio.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wglobal-constructors"
// NOLINTNEXTLINE
vull::Vector<Test *> g_tests;
#pragma clang diagnostic pop

int main() {
    bool any_failed = false;
    for (auto *test : g_tests) {
        printf(" RUN %s\n", test->name().data());
        try {
            test->fn()();
            printf("  OK %s\n", test->name().data());
        } catch (const TestFailure &failure) {
            any_failed = true;
            printf("FAIL %s\n", test->name().data());
            printf("     '%s'\n", failure.expr());
            printf("     failed at %s:%u\n", failure.file(), failure.line());
        }
    }
    return any_failed ? 1 : 0;
}
