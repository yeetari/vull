#include <vull/container/vector.hh>
#include <vull/core/log.hh>
#include <vull/core/main.hh>
#include <vull/platform/file.hh>
#include <vull/support/assert.hh>
#include <vull/support/optional.hh>
#include <vull/support/string.hh>
#include <vull/support/string_view.hh>
#include <vull/support/utility.hh>
#include <vull/tasklet/scheduler.hh>
#include <vull/vpak/file_system.hh>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

using namespace vull;

namespace {

Optional<String> parse_args(int argc, char **argv, Vector<StringView> &application_args) {
    Vector<StringView> args(argv, argv + argc);

    bool next_is_vpak_dir = false;
    StringView vpak_dir;
    for (const auto arg : args) {
        if (arg == "--vpak-dir") {
            next_is_vpak_dir = true;
        } else if (next_is_vpak_dir) {
            next_is_vpak_dir = false;
            vpak_dir = arg;
        } else {
            application_args.push(arg);
        }
    }

    if (next_is_vpak_dir) {
        vull::println("fatal: missing argument to --vpak-dir");
        return {};
    }

    if (!vpak_dir.empty()) {
        return String(vpak_dir);
    }

    char *last_slash = argv[0];
    for (char *path = argv[0]; *path != '\0'; path++) {
        if (*path == '/') {
            last_slash = path;
        }
    }
    return String::copy_raw(argv[0], static_cast<size_t>(last_slash - argv[0]));
}

} // namespace

int main(int argc, char **argv) {
    vull::open_log();
    vull::set_log_colours_enabled(isatty(STDOUT_FILENO) == 1);

    Vector<StringView> application_args;
    auto vpak_directory_path = parse_args(argc, argv, application_args);
    if (!vpak_directory_path) {
        return 1;
    }

    DIR *dir = opendir(vpak_directory_path->data());
    int dir_fd = open(vpak_directory_path->data(), O_DIRECTORY);
    if (dir == nullptr || dir_fd < 0) {
        vull::println("fatal: failed to open vpak directory {}", *vpak_directory_path);
        return 1;
    }

    // TODO: Sort alphabetically to allow overriding entries.
    dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        String name(static_cast<const char *>(entry->d_name));
        if (name.ends_with(".vpak")) {
            vull::info("[main] Found vpak {}", name);
            int fd = openat(dir_fd, name.data(), 0);
            if (fd < 0) {
                vull::error("[main] Failed to open {}: {}", name, strerror(errno));
                continue;
            }
            vpak::load_vpak(File::from_fd(fd));
        }
    }
    close(dir_fd);
    closedir(dir);

    Scheduler scheduler;
    scheduler.start([args = vull::move(application_args)]() mutable {
        vull_main(vull::move(args));
        Scheduler::current().stop();
    });
}
