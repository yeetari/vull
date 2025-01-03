#include <vull/core/application.hh>

#include <vull/core/log.hh>
#include <vull/platform/file.hh>
#include <vull/support/args_parser.hh>
#include <vull/support/function.hh>
#include <vull/support/string.hh>
#include <vull/support/string_view.hh>
#include <vull/support/utility.hh>
#include <vull/tasklet/scheduler.hh>
#include <vull/vpak/file_system.hh>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

namespace vull {

static int vpak_select(const struct dirent *entry) {
    return StringView(static_cast<const char *>(entry->d_name)).ends_with(".vpak") ? 1 : 0;
}

int start_application(int argc, char **argv, ArgsParser &args_parser, Function<void()> start_fn) {
    String vpak_directory_path;
    args_parser.add_option(vpak_directory_path, "Vpak directory path", "vpak-dir");
    if (auto result = args_parser.parse_args(argc, argv); result != ArgsParseResult::Continue) {
        return result == ArgsParseResult::ExitSuccess ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    // Default to directory containing the executable.
    if (vpak_directory_path.empty()) {
        vpak_directory_path = vull::dir_path(argv[0]);
    }

    vull::open_log();
    vull::set_log_colours_enabled(isatty(STDOUT_FILENO) == 1);

    struct dirent **entry_list;
    int entry_count = scandir(vpak_directory_path.data(), &entry_list, &vpak_select, &alphasort);
    if (entry_count < 0) {
        vull::error("[main] Failed to scan vpak directory '{}': {}", vpak_directory_path, strerror(errno));
        return EXIT_FAILURE;
    }

    int dir_fd = open(vpak_directory_path.data(), O_DIRECTORY);
    if (dir_fd < 0) {
        vull::error("[main] Failed to open vpak directory '{}': {}", vpak_directory_path, strerror(errno));
        return EXIT_FAILURE;
    }

    for (int i = 0; i < entry_count; i++) {
        const dirent *entry = entry_list[i];

        String name(static_cast<const char *>(entry->d_name));
        vull::info("[main] Found vpak {}", name);

        int fd = openat(dir_fd, name.data(), 0);
        if (fd < 0) {
            vull::error("[main] Failed to open vpak '{}': {}", name, strerror(errno));
            return EXIT_FAILURE;
        }

        vpak::load_vpak(File::from_fd(fd));
        free(entry_list[i]);
    }
    free(entry_list);
    close(dir_fd);

    Scheduler scheduler;
    scheduler.start([start_fn = vull::move(start_fn)] {
        start_fn();
        Scheduler::current().stop();
        vull::close_log();
    });
    return EXIT_SUCCESS;
}

} // namespace vull
