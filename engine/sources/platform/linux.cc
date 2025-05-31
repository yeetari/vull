#include <vull/platform/file.hh>
#include <vull/platform/file_stream.hh>
#include <vull/platform/system_latch.hh>
#include <vull/platform/system_mutex.hh>
#include <vull/platform/system_semaphore.hh>
#include <vull/platform/timer.hh>

#include <vull/container/array.hh>
#include <vull/container/vector.hh>
#include <vull/support/algorithm.hh>
#include <vull/support/atomic.hh>
#include <vull/support/result.hh>
#include <vull/support/span.hh>
#include <vull/support/stream.hh>
#include <vull/support/string.hh>
#include <vull/support/string_builder.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>

#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <linux/futex.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

namespace vull {

File::~File() {
    if (m_fd >= 0) {
        close(m_fd);
    }
}

File &File::operator=(File &&other) {
    File moved(vull::move(other));
    vull::swap(m_fd, moved.m_fd);
    return *this;
}

FileStream File::create_stream() const {
    struct stat stat_buf{};
    fstat(m_fd, &stat_buf);
    return {dup(m_fd), (stat_buf.st_mode & S_IFREG) != 0};
}

Result<void, FileError> File::copy_to(const File &target, int64_t &src_offset, int64_t &dst_offset) const {
    struct stat stat_buf{};
    if (fstat(m_fd, &stat_buf) < 0) {
        return FileError::Unknown;
    }
    const auto size = static_cast<size_t>(stat_buf.st_size);
    if (copy_file_range(m_fd, &src_offset, target.fd(), &dst_offset, size, 0) < 0) {
        return FileError::Unknown;
    }
    return {};
}

Result<void, FileError> File::link_to(String path) const {
    // linkat with AT_EMPTY_PATH requires a capability, so use procfs instead.
    auto fd_path = vull::format("/proc/self/fd/{}", m_fd);
    int rc = linkat(AT_FDCWD, fd_path.data(), AT_FDCWD, path.data(), AT_SYMLINK_FOLLOW);
    if (rc < 0) {
        return FileError::Unknown;
    }
    return {};
}

Result<void, FileError> File::sync() const {
    if (fsync(m_fd) < 0) {
        return FileError::Unknown;
    }
    return {};
}

String dir_path(String path) {
    return dirname(path.data());
}

Result<void, FileError> unlink_path(String path) {
    int rc = unlink(path.data());
    if (rc < 0) {
        if (errno == ENOENT) {
            return FileError::NonExistent;
        }
        return FileError::Unknown;
    }
    return {};
}

Result<File, OpenError> open_file(String path, OpenMode mode) {
    int flags = O_RDONLY;
    if ((mode & OpenMode::Read) != OpenMode::None && (mode & OpenMode::Write) != OpenMode::None) {
        flags = O_RDWR;
    } else if ((mode & OpenMode::Write) != OpenMode::None) {
        flags = O_WRONLY;
    }

    flags |= O_CLOEXEC;
    if ((mode & OpenMode::Create) != OpenMode::None) {
        flags |= O_CREAT;
    }
    if ((mode & OpenMode::Directory) != OpenMode::None) {
        flags |= O_DIRECTORY;
    }
    if ((mode & OpenMode::Truncate) != OpenMode::None) {
        flags |= O_TRUNC;
    }
    if ((mode & OpenMode::TempFile) != OpenMode::None) {
        flags |= O_TMPFILE;
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

Result<void, FileError> read_entire_file(String path, Vector<uint8_t> &bytes) {
    // TODO: Could optimise by stat'ing and ensuring vectory capacity.
    int fd = open(path.data(), O_RDONLY);
    if (fd < 0) {
        if (errno == EACCES) {
            return FileError::BadAccess;
        }
        if (errno == ENOENT) {
            return FileError::NonExistent;
        }
        return FileError::Unknown;
    }

    uint32_t bytes_read = 0;
    do {
        Array<uint8_t, 16384> temp{};
        ssize_t rc = read(fd, temp.data(), temp.size());
        if (rc < 0) {
            close(fd);
            return FileError::Unknown;
        }
        bytes_read = static_cast<uint32_t>(rc);
        vull::copy(temp.begin(), temp.begin() + bytes_read, vull::back_inserter(bytes));
    } while (bytes_read != 0);
    close(fd);
    return {};
}

Result<String, FileError> read_entire_file_ascii(String path) {
    Vector<uint8_t> bytes;
    VULL_TRY(vull::read_entire_file(vull::move(path), bytes));

    // Ensure nul-terminated.
    if (bytes.empty() || bytes.last() != 0) {
        bytes.push(0);
    }

    // Interpret raw bytes directly as ASCII.
    auto span = bytes.take_all();
    return String::move_raw(vull::bit_cast<char *>(span.data()), span.size() - 1);
}

FileStream::~FileStream() {
    if (m_fd >= 0) {
        close(m_fd);
    }
}

UniquePtr<Stream> FileStream::clone_unique() const {
    return vull::make_unique<FileStream>(FileStream(dup(m_fd), m_seekable));
}

Result<size_t, StreamError> FileStream::seek(StreamOffset offset, SeekMode mode) {
    if (!m_seekable) {
        return StreamError::NotImplemented;
    }
    switch (mode) {
    case SeekMode::Set:
        m_head = static_cast<size_t>(offset);
        break;
    case SeekMode::Add:
        // TODO: Check for overflow.
        m_head = static_cast<size_t>(static_cast<ssize_t>(m_head) + offset);
        break;
    case SeekMode::End: {
        struct stat stat_buf{};
        if (fstat(m_fd, &stat_buf) < 0) {
            return StreamError::Unknown;
        }
        m_head = static_cast<size_t>(stat_buf.st_size + offset);
        break;
    }
    }
    return m_head;
}

Result<size_t, StreamError> FileStream::read(Span<void> data) {
    ssize_t rc;
    if (m_seekable) {
        rc = pread(m_fd, data.data(), data.size(), static_cast<off_t>(m_head));
    } else {
        rc = ::read(m_fd, data.data(), data.size());
    }
    if (rc < 0) {
        return StreamError::Unknown;
    }

    const auto bytes_read = static_cast<size_t>(rc);
    m_head += bytes_read;
    return bytes_read;
}

Result<void, StreamError> FileStream::write(Span<const void> data) {
    ssize_t rc;
    if (m_seekable) {
        rc = pwrite(m_fd, data.data(), data.size(), static_cast<off_t>(m_head));
    } else {
        rc = ::write(m_fd, data.data(), data.size());
    }
    if (rc < 0) {
        return StreamError::Unknown;
    }

    const auto bytes_written = static_cast<size_t>(rc);
    m_head += bytes_written;
    if (bytes_written != data.size()) {
        return StreamError::Truncated;
    }
    return {};
}

void SystemLatch::count_down() {
    if (m_value.fetch_sub(1) == 1) {
        syscall(SYS_futex, m_value.raw_ptr(), FUTEX_WAKE_PRIVATE, UINT32_MAX, nullptr, nullptr, 0);
    }
}

void SystemLatch::wait() {
    uint32_t value;
    while ((value = m_value.load()) != 0) {
        syscall(SYS_futex, m_value.raw_ptr(), FUTEX_WAIT_PRIVATE, value, nullptr, nullptr, 0);
    }
}

void SystemMutex::lock() {
    auto state = State::Unlocked;
    if (m_state.compare_exchange(state, State::Locked)) [[likely]] {
        // Successfully locked the mutex.
        return;
    }

    do {
        // Signal that the mutex now has waiters (first check avoids the cmpxchg if unnecessary).
        if (state == State::LockedWaiters || m_state.cmpxchg(State::Locked, State::LockedWaiters) != State::Unlocked) {
            // Wait on the mutex to unlock. A spurious wakeup is fine here since the loop will just reiterate.
            syscall(SYS_futex, m_state.raw_ptr(), FUTEX_WAIT_PRIVATE, State::LockedWaiters, nullptr, nullptr, 0);
        }
    } while ((state = m_state.cmpxchg(State::Unlocked, State::LockedWaiters)) != State::Unlocked);
}

void SystemMutex::unlock() {
    if (m_state.exchange(State::Unlocked) == State::LockedWaiters) {
        // Wake one waiter.
        syscall(SYS_futex, m_state.raw_ptr(), FUTEX_WAKE_PRIVATE, 1, nullptr, nullptr, 0);
    }
}

void SystemSemaphore::post() {
    uint64_t data = m_data.fetch_add(1, vull::memory_order_release);
    if ((data >> 32) > 0) {
        // Wake one blocked waiter.
        auto *value = vull::bit_cast<uint32_t *>(m_data.raw_ptr());
#if BYTE_ORDER == BIG_ENDIAN
        value++;
#endif
        syscall(SYS_futex, value, FUTEX_WAKE_PRIVATE, 1, nullptr, nullptr, 0);
    }
}

void SystemSemaphore::wait() {
    // Try to acquire the semaphore.
    uint64_t data = m_data.load(vull::memory_order_relaxed);
    if ((data & 0xffffffffu) != 0 && m_data.compare_exchange_weak(data, data - 1, vull::memory_order_acquire)) {
        return;
    }

    // Otherwise add a waiter.
    m_data.fetch_add(1ul << 32, vull::memory_order_relaxed);

    while (true) {
        if ((data & 0xffffffffu) == 0) {
            // Sleep until a post wakes us up.
            auto *value = vull::bit_cast<uint32_t *>(m_data.raw_ptr());
#if BYTE_ORDER == BIG_ENDIAN
            value++;
#endif
            syscall(SYS_futex, value, FUTEX_WAIT_PRIVATE, 0, nullptr, nullptr, 0);
            data = m_data.load(vull::memory_order_relaxed);
        } else {
            // Try to acquire the semaphore and stop being a waiter.
            const auto new_data = data - 1 - (1ul << 32);
            if (m_data.compare_exchange_weak(data, new_data, vull::memory_order_acquire)) {
                return;
            }
        }
    }
}

static uint64_t monotonic_time() {
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return uint64_t(ts.tv_sec) * 1000000000ull + uint64_t(ts.tv_nsec);
}

Timer::Timer() : m_epoch(monotonic_time()) {}

float Timer::elapsed() const {
    return static_cast<float>(monotonic_time() - m_epoch) / 1000000000.0f;
}

uint64_t Timer::elapsed_ns() const {
    return monotonic_time() - m_epoch;
}

void Timer::reset() {
    m_epoch = monotonic_time();
}

} // namespace vull
