#pragma once

#include <vull/support/result.hh>
#include <vull/support/utility.hh>

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

namespace vull {

enum class ThreadError {
    InsufficientPermission,
    InsufficientResources,
    Unknown,
};

class Thread {
    pthread_t m_thread{};

    static Result<pthread_t, ThreadError> create(void *(*function)(void *), void *argument);
    template <typename F>
    static void *proxy(void *argument);

    explicit Thread(pthread_t thread) : m_thread(thread) {}

public:
    template <typename F>
    static Result<Thread, ThreadError> create(F &&callable);

    Thread() = default;
    Thread(const Thread &) = delete;
    Thread(Thread &&other) : m_thread(vull::exchange(other.m_thread, {})) {}
    ~Thread();

    Thread &operator=(const Thread &) = delete;
    Thread &operator=(Thread &&);

    /**
     * Blocks all non-fault signal handlers on the current thread and any child threads.
     */
    static Result<void, ThreadError> block_signals();
    static Result<void, ThreadError> setup_signal_stack();
    [[noreturn]] static void exit();
    static void yield();
    Result<void, ThreadError> join();
    Result<void, ThreadError> pin_to_core(size_t core) const;
    Result<void, ThreadError> set_idle() const;
};

template <typename F>
void *Thread::proxy(void *argument) {
    // Move function to the stack in case the thread terminates early.
    auto function = vull::move(*static_cast<F *>(argument));
    delete static_cast<F *>(argument);
    function();
    return nullptr;
}

template <typename F>
Result<Thread, ThreadError> Thread::create(F &&callable) {
    return Thread(VULL_TRY(create(&proxy<F>, new F(vull::move(callable)))));
}

void install_fault_handler();

} // namespace vull
