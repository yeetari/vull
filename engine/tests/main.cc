#include <vull/core/Log.hh>
#include <vull/support/Algorithm.hh>
#include <vull/support/Span.hh>
#include <vull/support/String.hh>
#include <vull/support/StringBuilder.hh>
#include <vull/support/StringView.hh>
#include <vull/support/Test.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>

#include <stdlib.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wglobal-constructors"
// NOLINTNEXTLINE
vull::Vector<Test *> g_tests;
#pragma clang diagnostic pop

namespace {

void print_usage(vull::StringView prog) {
    vull::StringBuilder sb;

    sb.append("`{}' runs automated tests\n\n", prog);
    sb.append("usage: {} [--verbose] [--list-tests] [TEST]...\n\n", prog);
    sb.append("  --verbose     print skipped tests\n");
    sb.append("  --list-tests  print the name of all known tests\n");
    sb.append("  TEST          run only tests named TEST. can be repeated\n");

    vull::println(sb.build());
}

} // namespace

int main(int argc, char **argv) {
    vull::Vector<vull::StringView> args(argv, argv + argc);

    // Ensure that the program name is not empty. Someone rude (i.e. not a shell) may call with empty argv.
    if (args.empty()) {
        args.push("vull-tests");
    }

    bool verbose = false;
    bool list_tests = false;
    vull::Vector<vull::StringView> filter;

    for (auto sv : vull::slice(args, 1u)) {
        if (sv == "--help" || sv == "-h") {
            print_usage(args[0]);
            return EXIT_SUCCESS;
        }

        if (sv == "--verbose") {
            verbose = true;
        } else if (sv == "--list-tests") {
            list_tests = true;
        } else if (!sv.empty() && sv[0] == '-') {
            vull::println("unknown argument `{}'", sv);
            return EXIT_FAILURE;
        } else {
            filter.push(sv);
        }
    }

    if (list_tests) {
        for (auto *test : g_tests) {
            vull::println("{}", test->name());
        }
        return EXIT_SUCCESS;
    }

    bool any_failed = false;
    for (auto *test : g_tests) {
        if (filter.empty() || vull::contains(filter, static_cast<vull::StringView>(test->name()))) {
            vull::print("RUN {}... ", test->name());
            try {
                test->fn()();
                vull::println("OK");
            } catch (const TestFailure &failure) {
                any_failed = true;
                vull::println("FAIL");
                vull::println("    '{}' at {}:{}", failure.expr(), failure.file(), failure.line());
            }
        } else if (verbose) {
            vull::println(" SKIP {}\n", test->name());
        }
    }

    return any_failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
