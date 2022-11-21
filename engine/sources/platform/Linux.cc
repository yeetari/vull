#include <vull/platform/File.hh>
#include <vull/platform/FileStream.hh>
#include <vull/platform/Latch.hh>
#include <vull/platform/Mutex.hh>
#include <vull/platform/Timer.hh>

#include <vull/support/Array.hh>
#include <vull/support/Atomic.hh>
#include <vull/support/Result.hh>
#include <vull/support/Span.hh>
#include <vull/support/StreamError.hh>
#include <vull/support/String.hh>

#include <errno.h>
#include <fcntl.h>
#include <linux/futex.h>
#include <stdint.h>
#include <syscall.h>
#include <time.h>
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

Result<void, StreamError> FileStream::read(Span<void> data) {
    ssize_t rc = pread(m_fd, data.data(), data.size(), static_cast<off_t>(m_head));
    if (rc < 0) {
        return StreamError::Unknown;
    }

    const auto bytes_read = static_cast<size_t>(rc);
    m_head += bytes_read;
    if (bytes_read != data.size()) {
        return StreamError::Truncated;
    }
    return {};
}

Result<void, StreamError> FileStream::write(Span<const void> data) {
    ssize_t rc = pwrite(m_fd, data.data(), data.size(), static_cast<off_t>(m_head));
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

void Latch::count_down() {
    if (m_value.fetch_sub(1) == 1) {
        syscall(SYS_futex, m_value.raw_ptr(), FUTEX_WAKE_PRIVATE, UINT32_MAX, nullptr, nullptr, 0);
    }
}

void Latch::wait() {
    uint32_t value;
    while ((value = m_value.load()) != 0) {
        syscall(SYS_futex, m_value.raw_ptr(), FUTEX_WAIT_PRIVATE, value, nullptr, nullptr, 0);
    }
}

void Mutex::lock() {
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

void Mutex::unlock() {
    if (m_state.exchange(State::Unlocked) == State::LockedWaiters) {
        // Wake 1 waiter.
        syscall(SYS_futex, m_state.raw_ptr(), FUTEX_WAKE_PRIVATE, 1, nullptr, nullptr, 0);
    }
}

static uint64_t monotonic_time() {
    struct timespec ts {};
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
