#include <vull/core/application.hh>

#include <vull/core/log.hh>
#include <vull/platform/file.hh>
#include <vull/platform/thread.hh>
#include <vull/support/args_parser.hh>
#include <vull/support/function.hh>
#include <vull/support/result.hh>
#include <vull/support/string.hh>
#include <vull/support/string_builder.hh>
#include <vull/support/string_view.hh>
#include <vull/support/utility.hh>
#include <vull/tasklet/scheduler.hh>
#include <vull/tasklet/tasklet.hh>
#include <vull/vpak/file_system.hh>

#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

namespace vull {

static int vpak_select(const struct dirent *entry) {
    return StringView(static_cast<const char *>(entry->d_name)).ends_with(".vpak") ? 1 : 0;
}

int start_application(int argc, char **argv, ArgsParser &args_parser, Function<void()> start_fn) {
    uint32_t thread_count = 0;
    String vpak_directory_path;
    args_parser.add_option(thread_count, "Tasklet worker thread count", "threads");
    args_parser.add_option(vpak_directory_path, "Vpak directory path", "vpak-dir");
    if (auto result = args_parser.parse_args(argc, argv); result != ArgsParseResult::Continue) {
        return result == ArgsParseResult::ExitSuccess ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    // Install fault handler and block conventional signal handlers for the whole process.
    // TODO: Use signalfd to handle SIGINT, SIGQUIT, etc.
    platform::install_fault_handler();
    if (platform::Thread::block_signals().is_error()) {
        vull::error("[main] Failed to mask signals");
    }
    VULL_IGNORE(platform::Thread::current().set_name("Main thread"));

    // Default to directory containing the executable.
    if (vpak_directory_path.empty()) {
        vpak_directory_path = platform::dir_path(argv[0]);
    }

    vull::open_log();
    vull::set_log_colours_enabled(isatty(STDOUT_FILENO) == 1);

    struct dirent **entry_list;
    int entry_count = scandir(vpak_directory_path.data(), &entry_list, &vpak_select, &alphasort);
    if (entry_count < 0) {
        vull::error("[main] Failed to scan vpak directory '{}': {}", vpak_directory_path, strerror(errno));
        return EXIT_FAILURE;
    }

    for (int i = 0; i < entry_count; i++) {
        StringView name(static_cast<const char *>(entry_list[i]->d_name));
        vpak::load_vpak(name, vull::format("{}/{}", vpak_directory_path, name));
        free(entry_list[i]);
    }
    free(entry_list);

    tasklet::Scheduler scheduler(thread_count);
    scheduler.run<tasklet::TaskletSize::Large>(vull::move(start_fn));
    scheduler.join();
    vull::close_log();
    return EXIT_SUCCESS;
}

} // namespace vull
