#include <vull/platform/file.hh>
#include <vull/platform/file_stream.hh>
#include <vull/platform/system_latch.hh>
#include <vull/platform/system_mutex.hh>
#include <vull/platform/system_semaphore.hh>
#include <vull/platform/thread.hh>
#include <vull/platform/timer.hh>

#include <vull/container/array.hh>
#include <vull/container/vector.hh>
#include <vull/support/algorithm.hh>
#include <vull/support/assert.hh>
#include <vull/support/atomic.hh>
#include <vull/support/result.hh>
#include <vull/support/span.hh>
#include <vull/support/stream.hh>
#include <vull/support/string.hh>
#include <vull/support/string_builder.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>
#include <vull/tasklet/tasklet.hh>

// IWYU pragma: no_include <bits/types/stack_t.h>
// IWYU pragma: no_include <bits/types/struct_sched_param.h>
#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <linux/futex.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

namespace vull {

#if VULL_ASAN_ENABLED
// NOLINTNEXTLINE
extern "C" const char *__asan_default_options() {
    // Prevent asan from messing with our alternate signal stacks.
    return "use_sigaltstack=0";
}
#endif

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

void SystemSemaphore::release() {
    m_data.fetch_or(0xffffffu, vull::memory_order_release);
    auto *value = vull::bit_cast<uint32_t *>(m_data.raw_ptr());
#if BYTE_ORDER == BIG_ENDIAN
    value++;
#endif
    syscall(SYS_futex, value, FUTEX_WAKE_PRIVATE, UINT32_MAX, nullptr, nullptr, 0);
}

bool SystemSemaphore::try_wait() {
    uint64_t data = m_data.load(vull::memory_order_relaxed);
    while (true) {
        // Check whether the semaphore has been signaled and return if it hasn't.
        if ((data & 0xffffffffu) == 0) {
            return false;
        }

        // The semaphore is signaled so try to decrement it. Use a weak compare exchange since we need the loop anyway
        // to prevent any race from returning a false negative of the semaphore not being signaled.
        if (m_data.compare_exchange_weak(data, data - 1, vull::memory_order_acquire)) {
            return true;
        }
    }
}

void SystemSemaphore::wait() {
    // Try to acquire the semaphore. Use a single weak compare exchange since a race or spurious failure won't cause
    // any semantic problems here.
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

Result<pthread_t, ThreadError> Thread::create(void *(*function)(void *), void *argument) {
    pthread_t thread;
    const int rc = pthread_create(&thread, nullptr, function, argument);
    switch (rc) {
    case 0:
        return thread;
    case EAGAIN:
        return ThreadError::InsufficientResources;
    case EPERM:
        return ThreadError::InsufficientPermission;
    default:
        return ThreadError::Unknown;
    }
}

Thread::~Thread() {
    VULL_EXPECT(join());
}

Thread &Thread::operator=(Thread &&other) {
    VULL_ASSERT(m_thread == 0);
    m_thread = vull::exchange(other.m_thread, {});
    return *this;
}

static constexpr Array k_fault_signals{SIGBUS, SIGFPE, SIGILL, SIGSEGV, SIGTRAP};

Result<void, ThreadError> Thread::block_signals() {
    sigset_t sig_set;
    if (sigfillset(&sig_set) != 0) {
        return ThreadError::Unknown;
    }
    for (auto signal : k_fault_signals) {
        if (sigdelset(&sig_set, signal) != 0) {
            return ThreadError::Unknown;
        }
    }
    if (pthread_sigmask(SIG_BLOCK, &sig_set, nullptr) != 0) {
        return ThreadError::Unknown;
    }
    return {};
}

VULL_GLOBAL(static thread_local uint8_t *s_signal_stack = nullptr);

Result<void, ThreadError> Thread::setup_signal_stack() {
    const auto size = static_cast<size_t>(SIGSTKSZ);
    s_signal_stack = new uint8_t[size];
    stack_t new_stack{
        .ss_sp = s_signal_stack,
        .ss_size = size,
    };
    if (sigaltstack(&new_stack, nullptr) != 0) {
        return ThreadError::Unknown;
    }
    return {};
}

[[noreturn]] void Thread::exit() {
    delete[] s_signal_stack;
    pthread_exit(nullptr);
}

Result<void, ThreadError> Thread::join() {
    if (m_thread != 0 && pthread_join(m_thread, nullptr) != 0) {
        return ThreadError::Unknown;
    }
    m_thread = 0;
    return {};
}

Result<void, ThreadError> Thread::pin_to_core(size_t core) const {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(core, &set);
    if (pthread_setaffinity_np(m_thread, sizeof(cpu_set_t), &set) != 0) {
        return ThreadError::Unknown;
    }
    return {};
}

Result<void, ThreadError> Thread::set_idle() const {
    sched_param param{};
    int rc = pthread_setschedparam(m_thread, SCHED_IDLE, &param);
    switch (rc) {
    case 0:
        return {};
    case EPERM:
        return ThreadError::InsufficientPermission;
    default:
        return ThreadError::Unknown;
    }
}

[[noreturn]] static void fault_handler(int signal, siginfo_t *info, void *) {
    const auto address = vull::bit_cast<uintptr_t>(info->si_addr);
    const auto *tasklet = tasklet::Tasklet::current();
    const char *signal_name = sigabbrev_np(signal);
    if (tasklet != nullptr && tasklet->is_guard_page(address)) {
        fprintf(stderr, "Stack overflow at address 0x%lx in tasklet %p\n", address, static_cast<const void *>(tasklet));
    } else if (tasklet != nullptr) {
        fprintf(stderr, "SIG%s at address 0x%lx in tasklet %p\n", signal_name, address,
                static_cast<const void *>(tasklet));
    } else {
        fprintf(stderr, "SIG%s at address 0x%lx\n", signal_name, address);
    }
    _Exit(EXIT_FAILURE);
}

void install_fault_handler() {
    struct sigaction action{
        .sa_flags = SA_SIGINFO | SA_ONSTACK,
    };
    sigemptyset(&action.sa_mask);
    action.sa_sigaction = &fault_handler;
    for (auto signal : k_fault_signals) {
        sigaction(signal, &action, nullptr);
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
