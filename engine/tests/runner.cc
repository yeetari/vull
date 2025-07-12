#include <vull/container/vector.hh>
#include <vull/core/log.hh>
#include <vull/support/algorithm.hh>
#include <vull/support/args_parser.hh>
#include <vull/support/string.hh>
#include <vull/support/string_view.hh>
#include <vull/support/utility.hh>
#include <vull/test/test.hh>

#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>

using namespace vull;
using namespace vull::test;

// NOLINTBEGIN: these are defined by the linker
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreserved-identifier"
extern Test __start_vull_test_info;
extern Test __stop_vull_test_info;
#pragma clang diagnostic pop
// NOLINTEND

namespace vull::test {

VULL_GLOBAL(Test *g_current_test);
VULL_GLOBAL(static pthread_mutex_t s_mutex);

void Test::append_message(String &&message) {
    pthread_mutex_lock(&s_mutex);
    messages.push(vull::move(message));
    pthread_mutex_unlock(&s_mutex);
}

} // namespace vull::test

int main(int argc, char **argv) {
    bool verbose = false;
    bool list_tests = false;
    Vector<String> test_filter;

    ArgsParser args_parser("vull-tests", "Vull Test Runner", "0.1.0");
    args_parser.add_flag(verbose, "Print skipped tests", "verbose", 'v');
    args_parser.add_flag(list_tests, "Print all known tests", "list-tests");
    args_parser.add_argument(test_filter, "test", false);
    if (auto result = args_parser.parse_args(argc, argv); result != ArgsParseResult::Continue) {
        return result == ArgsParseResult::ExitSuccess ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    const auto tests = vull::make_range(&__start_vull_test_info, &__stop_vull_test_info);
    if (list_tests) {
        for (const auto &test : tests) {
            vull::println(test.name);
        }
        return EXIT_SUCCESS;
    }

    uint32_t passed_count = 0;
    uint32_t failed_count = 0;
    pthread_mutex_init(&s_mutex, nullptr);
    for (auto &test : tests) {
        g_current_test = &test;
        if (test_filter.empty() || vull::contains(test_filter, test.name)) {
            vull::print("RUN  {}... ", test.name);
            test.fn();

            const bool passed = test.messages.empty();
            if (passed) {
                passed_count++;
            } else {
                failed_count++;
            }

            vull::println(passed ? "PASS" : "FAIL");
            for (const auto &message : test.messages) {
                vull::println(message);
            }
        } else if (verbose) {
            vull::println("SKIP {}", test.name);
        }
    }

    vull::println("{} tests ran, {} passed, {} failed", passed_count + failed_count, passed_count, failed_count);
    return failed_count > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
