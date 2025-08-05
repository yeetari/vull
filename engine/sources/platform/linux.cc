#include <vull/platform/event.hh>
#include <vull/platform/file.hh>
#include <vull/platform/file_stream.hh>
#include <vull/platform/platform.hh>
#include <vull/platform/semaphore.hh>
#include <vull/platform/tasklet.hh>
#include <vull/platform/thread.hh>
#include <vull/platform/timer.hh>

#include <vull/container/array.hh>
#include <vull/container/vector.hh>
#include <vull/core/log.hh>
#include <vull/core/tracing.hh>
#include <vull/maths/common.hh>
#include <vull/support/algorithm.hh>
#include <vull/support/assert.hh>
#include <vull/support/atomic.hh>
#include <vull/support/function.hh>
#include <vull/support/result.hh>
#include <vull/support/span.hh>
#include <vull/support/stream.hh>
#include <vull/support/string.hh>
#include <vull/support/string_builder.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>
#include <vull/tasklet/fiber.hh>
#include <vull/tasklet/future.hh>
#include <vull/tasklet/io.hh>
#include <vull/tasklet/scheduler.hh>

#ifdef VULL_BUILD_GRAPHICS
#include <vull/vulkan/fence.hh>
#endif

// IWYU pragma: no_include <bits/types/stack_t.h>
// IWYU pragma: no_include <bits/types/struct_sched_param.h>
#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <liburing.h>
#include <liburing/io_uring.h>
#include <linux/futex.h>
#include <poll.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <sys/signalfd.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

namespace vull::platform {

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

Result<File, OpenError> open_file(String path, OpenModes modes) {
    int flags = O_RDONLY;
    if (modes.is_set(OpenMode::Read) && modes.is_set(OpenMode::Write)) {
        flags = O_RDWR;
    } else if (modes.is_set(OpenMode::Write)) {
        flags = O_WRONLY;
    }

    flags |= O_CLOEXEC;
    if (modes.is_set(OpenMode::Create)) {
        flags |= O_CREAT;
    }
    if (modes.is_set(OpenMode::Directory)) {
        flags |= O_DIRECTORY;
    }
    if (modes.is_set(OpenMode::Truncate)) {
        flags |= O_TRUNC;
    }
    if (modes.is_set(OpenMode::TempFile)) {
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
    VULL_TRY(platform::read_entire_file(vull::move(path), bytes));

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

Event::Event() {
    m_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
}

Event::~Event() {
    close(m_fd);
}

void Event::set() const {
    eventfd_t value = 1;
    VULL_IGNORE(write(m_fd, &value, sizeof(eventfd_t)));
}

void Event::reset() const {
    eventfd_t value;
    VULL_IGNORE(read(m_fd, &value, sizeof(eventfd_t)));
}

void Event::wait() const {
    pollfd poll_fd{
        .fd = m_fd,
        .events = POLLIN,
    };
    poll(&poll_fd, 1, -1);
    reset();
}

void Semaphore::post() {
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

bool Semaphore::try_wait() {
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

void Semaphore::wait() {
    // Try to acquire the semaphore. Use a single weak compare exchange since a race or spurious failure won't cause
    // any semantic problems here.
    uint64_t data = m_data.load(vull::memory_order_relaxed);
    if ((data & 0xffffffffu) != 0 && m_data.compare_exchange_weak(data, data - 1, vull::memory_order_acquire)) {
        return;
    }

    // Otherwise add a waiter.
    m_data.fetch_add(1uz << 32, vull::memory_order_relaxed);

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
            const auto new_data = data - 1 - (1uz << 32);
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

Thread Thread::current() {
    return Thread(pthread_self());
}

Thread::~Thread() {
    if (m_thread != pthread_self()) {
        VULL_EXPECT(join());
    }
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

void Thread::yield() {
    sched_yield();
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

Result<void, ThreadError> Thread::set_name(String name) const {
    if (pthread_setname_np(m_thread, name.data()) != 0) {
        return ThreadError::Unknown;
    }
    return {};
}

uint32_t core_count() {
    return static_cast<uint32_t>(sysconf(_SC_NPROCESSORS_ONLN));
}

[[noreturn]] static void fault_handler(int signal, siginfo_t *info, void *) {
    const auto address = vull::bit_cast<uintptr_t>(info->si_addr);
    const auto *fiber = tasklet::Fiber::current();
    const char *signal_name = sigabbrev_np(signal);
    if (fiber != nullptr && fiber->is_guard_page(address)) {
        fprintf(stderr, "Stack overflow at address 0x%lx in tasklet %p\n", address,
                static_cast<const void *>(fiber->current_tasklet()));
    } else if (fiber != nullptr) {
        fprintf(stderr, "SIG%s at address 0x%lx in tasklet %p\n", signal_name, address,
                static_cast<const void *>(fiber->current_tasklet()));
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

static consteval int fiber_mmap_flags() {
    int flags = 0;

    // Not shared between processes.
    flags |= MAP_PRIVATE;

    // Not backed by a file.
    flags |= MAP_ANONYMOUS;

    // Advise that the mapping is for a stack. This seems to disable transparent huge pages.
    flags |= MAP_STACK;

    return flags;
}

uint8_t *allocate_fiber_memory(size_t size) {
    void *result = mmap(nullptr, size, PROT_READ | PROT_WRITE, fiber_mmap_flags(), -1, 0);
    if (result == MAP_FAILED) {
        return nullptr;
    }

    // Try to use MADV_GUARD_INSTALL.
    // TODO: Use the libc constant when it exists.
    const auto page_size = static_cast<size_t>(getpagesize());
    if (madvise(result, page_size, 102) < 0) {
        // Otherwise fallback to creating a PROT_NONE VMA.
        if (mprotect(result, page_size, PROT_NONE) < 0) {
            return nullptr;
        }
    }

    // Try to prefault in the first few pages. We don't really care if this fails.
    const auto prefault_amount = page_size * 4;
    auto *end = vull::bit_cast<uint8_t *>(result) + size;
    madvise(end - prefault_amount, prefault_amount, MADV_POPULATE_WRITE);
    return end;
}

static bool probe_flag(uint32_t flag) {
    io_uring ring;
    if (io_uring_queue_init(1, &ring, flag) == 0) {
        io_uring_queue_exit(&ring);
        return true;
    }
    return false;
}

static void queue_io_request(io_uring *ring, tasklet::IoRequest *request) {
    auto *sqe = io_uring_get_sqe(ring);
    io_uring_sqe_set_data(sqe, request);
    switch (request->kind()) {
        using enum tasklet::IoRequestKind;
    case Nop:
        io_uring_prep_nop(sqe);
        break;
    case PollEvent: {
        auto *poll_event = static_cast<tasklet::PollEventRequest *>(request);
        int fd = poll_event->event().fd();
        if (poll_event->multishot()) {
            io_uring_prep_poll_multishot(sqe, fd, POLLIN);
        } else {
            io_uring_prep_poll_add(sqe, fd, POLLIN);
        }
        break;
    }
    case WaitEvent: {
        auto *wait_event = static_cast<tasklet::WaitEventRequest *>(request);
        int fd = wait_event->event().fd();
        io_uring_prep_read(sqe, fd, &wait_event->value(), sizeof(eventfd_t), 0);
        break;
    }
#ifdef VULL_BUILD_GRAPHICS
    case WaitVkFence: {
        auto *wait_vk_fence = static_cast<tasklet::WaitVkFenceRequest *>(request);
        auto fd = wait_vk_fence->fence().make_fd();
        if (fd) {
            wait_vk_fence->set_fd(*fd);
            io_uring_prep_poll_add(sqe, *fd, POLLIN);
        } else {
            // Fence already signaled, just queue a nop.
            io_uring_prep_nop(sqe);
        }
        break;
    }
#endif
    }
}

void spawn_tasklet_io_dispatcher(tasklet::IoQueue &queue) {
    // Build ring flags.
    // TODO: Use IORING_SETUP_TASKRUN_FLAG?
    uint32_t ring_flags = 0;
    if (probe_flag(IORING_SETUP_SUBMIT_ALL)) {
        ring_flags |= IORING_SETUP_SUBMIT_ALL;
    } else {
        vull::warn("[platform] IORING_SETUP_SUBMIT_ALL is not supported");
    }
    if (probe_flag(IORING_SETUP_COOP_TASKRUN)) {
        ring_flags |= IORING_SETUP_COOP_TASKRUN;
    }
    if (probe_flag(IORING_SETUP_SINGLE_ISSUER)) {
        ring_flags |= IORING_SETUP_SINGLE_ISSUER;
    }

    io_uring ring;
    if (int rc = io_uring_queue_init(256, &ring, ring_flags); rc < 0) {
        vull::error("[platform] Failed to create io_uring: {}", strerror(-rc));
        vull::close_log();
        _Exit(1);
    }

    // Warn if nice features aren't supported.
    if ((ring.features & IORING_FEAT_NODROP) == 0) {
        vull::warn("[platform] IORING_FEAT_NODROP is not supported");
    }
    if ((ring.features & IORING_FEAT_FAST_POLL) == 0) {
        vull::warn("[platform] IORING_FEAT_FAST_POLL is not supported");
    }

    // Add the two events to the ring.
    // TODO: Would a multishot read be better?
    tasklet::PollEventRequest poll_quit_event(queue.quit_event, false);
    tasklet::PollEventRequest poll_submit_event(queue.submit_event, true);
    queue_io_request(&ring, &poll_quit_event);
    queue_io_request(&ring, &poll_submit_event);

    for (bool running = true; running;) {
        // Submit any pending SQEs and wait for at least one CQE.
        io_uring_submit_and_wait(&ring, 1);

        // Handle all ready CQEs.
        Array<io_uring_cqe *, 64> cqes;
        uint32_t cqe_count = io_uring_peek_batch_cqe(&ring, cqes.data(), cqes.size());
        for (uint32_t cqe_index = 0; cqe_index < cqe_count; cqe_index++) {
            auto *cqe = cqes[cqe_index];
            auto *request = static_cast<tasklet::IoRequest *>(io_uring_cqe_get_data(cqe));
            if (request == &poll_quit_event) {
                queue.quit_event.reset();
                running = false;
            } else if (request == &poll_submit_event) {
                // Requeue the event poll if the multishot was lost. This probably never happens.
                queue.submit_event.reset();
                if ((cqe->flags & IORING_CQE_F_MORE) == 0) {
                    queue_io_request(&ring, &poll_submit_event);
                }
            } else {
                // Close fence syncfd.
                if (request->kind() == tasklet::IoRequestKind::WaitVkFence) {
                    close(static_cast<tasklet::WaitVkFenceRequest *>(request)->fd());
                }

                // Resume the suspended tasklet.
                request->fulfill(cqe->res);
                request->sub_ref();
            }
        }
        io_uring_cq_advance(&ring, cqe_count);

        // Queue any new requests. This currently assumes that each IoRequest corresponds to one SQE.
        // TODO: Add functions to MpmcQueue optimised for SC dequeuing.
        const auto pending_count = queue.pending.exchange(0, vull::memory_order_relaxed);
        const auto to_queue_count = vull::min(pending_count, io_uring_sq_space_left(&ring));
        for (uint32_t i = 0; i < to_queue_count; i++) {
            queue_io_request(&ring, queue.dequeue(platform::Thread::yield));
        }

        // Resubmit the event if the amount we submitted was truncated.
        if (pending_count != to_queue_count) {
            queue.pending.fetch_add(pending_count - to_queue_count, vull::memory_order_relaxed);
            queue.submit_event.set();
        }
    }
    io_uring_queue_exit(&ring);
}

void take_over_main_thread(tasklet::Future<void> &&future, Function<void()> stop_fn) {
    // We can't poll on a future (futex), so use an event.
    Event event;
    future.and_then([&] {
        event.set();
    });

    sigset_t signal_mask;
    sigemptyset(&signal_mask);
    sigaddset(&signal_mask, SIGINT);
    sigaddset(&signal_mask, SIGQUIT);
    int signal_fd = signalfd(-1, &signal_mask, SFD_CLOEXEC);

    itimerspec timer_spec{
        {0, 1000000},
        {0, 1000000},
    };
    int timer_fd = -1;
    if (tracing::is_enabled()) {
        timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
        timerfd_settime(timer_fd, 0, &timer_spec, nullptr);
    }

    Array poll_fds{
        pollfd{
            .fd = event.fd(),
            .events = POLLIN,
        },
        pollfd{
            .fd = signal_fd,
            .events = POLLIN,
        },
        pollfd{
            .fd = timer_fd,
            .events = POLLIN,
        },
    };
    while (true) {
        poll(poll_fds.data(), poll_fds.size(), -1);

        // Check if the application has finished normally.
        if ((poll_fds[0].revents & POLLIN) != 0) {
            event.reset();
            break;
        }

        if ((poll_fds[1].revents & POLLIN) != 0) {
            signalfd_siginfo signal_info{};
            if (read(signal_fd, &signal_info, sizeof(signalfd_siginfo)) < 0) {
                vull::error("[platform] signalfd read failed");
                continue;
            }

            vull::debug("[platform] Received SIG{}", sigabbrev_np(static_cast<int>(signal_info.ssi_signo)));
            stop_fn();
        }

        if ((poll_fds[2].revents & POLLIN) != 0) {
            uint64_t timer_value;
            if (read(timer_fd, &timer_value, sizeof(uint64_t)) < 0) {
                vull::error("[platform] timerfd read failed");
            }
            tracing::plot_data("Queued Tasklet Count", tasklet::Scheduler::current().queued_tasklet_count());
        }
    }
}

void wake_address_single(uint32_t *address) {
    syscall(SYS_futex, address, FUTEX_WAKE_PRIVATE, 1, nullptr, nullptr, 0);
}

void wake_address_all(uint32_t *address) {
    syscall(SYS_futex, address, FUTEX_WAKE_PRIVATE, INT32_MAX, nullptr, nullptr, 0);
}

void wait_address(uint32_t *address, uint32_t expected) {
    syscall(SYS_futex, address, FUTEX_WAIT_PRIVATE, expected, nullptr, nullptr, 0);
}

static uint64_t monotonic_time() {
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return uint64_t(ts.tv_sec) * 1000000000uz + uint64_t(ts.tv_nsec);
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

} // namespace vull::platform
