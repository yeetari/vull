#include <vull/core/Log.hh>
#include <vull/core/Main.hh>
#include <vull/platform/File.hh>
#include <vull/platform/Timer.hh>
#include <vull/support/String.hh>
#include <vull/support/StringView.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>
#include <vull/tasklet/Scheduler.hh>
#include <vull/vpak/FileSystem.hh>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

using namespace vull;

int main(int argc, char **argv) {
    char *last_slash = argv[0];
    for (char *path = argv[0]; *path != '\0'; path++) {
        if (*path == '/') {
            last_slash = path;
        }
    }
    auto parent_path = String::copy_raw(argv[0], static_cast<size_t>(last_slash - argv[0]));
    DIR *dir = opendir(parent_path.data());
    int dir_fd = open(parent_path.data(), O_DIRECTORY);

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
    scheduler.start([=] {
        Vector<StringView> args(argv, argv + argc);
        vull_main(vull::move(args));
    });
}
