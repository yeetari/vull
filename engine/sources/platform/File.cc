#include <vull/platform/File.hh>

#include <vull/support/Array.hh>
#include <vull/support/Result.hh>
#include <vull/support/String.hh>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

namespace vull {

File::~File() {
    if (m_fd >= 0) {
        close(m_fd);
    }
}

Result<File, OpenError> open_file(String path, OpenMode mode) {
    int flags = O_RDONLY;
    if ((mode & OpenMode::Read) != OpenMode::None && (mode & OpenMode::Write) != OpenMode::None) {
        flags = O_RDWR;
    } else if ((mode & OpenMode::Write) != OpenMode::None) {
        flags = O_WRONLY;
    }

    if ((mode & OpenMode::Create) != OpenMode::None) {
        flags |= O_CREAT;
    }
    if ((mode & OpenMode::Truncate) != OpenMode::None) {
        flags |= O_TRUNC;
    }

    int rc = open(path.data(), flags, 0664);
    if (rc < 0) {
        if (errno == ENOENT) {
            return OpenError::NonExistent;
        }
        return OpenError::Unknown;
    }
    return File::from_fd(rc);
}

} // namespace vull
