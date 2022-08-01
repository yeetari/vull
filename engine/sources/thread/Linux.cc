#include <vull/thread/Latch.hh>
#include <vull/thread/Mutex.hh>

#include <vull/support/Atomic.hh>

#include <linux/futex.h>
#include <stdint.h>
#include <syscall.h>
#include <unistd.h>

namespace vull {

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

} // namespace vull
