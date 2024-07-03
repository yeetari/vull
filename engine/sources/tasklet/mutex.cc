#include <vull/tasklet/mutex.hh>

#include <vull/support/atomic.hh>
#include <vull/tasklet/tasklet.hh>

namespace vull {

[[gnu::noinline]] void Mutex::lock() {
    vull::atomic_thread_fence(vull::memory_order_seq_cst);
    auto state = State::Unlocked;
    if (m_state.compare_exchange(state, State::Locked, vull::memory_order_acq_rel, vull::memory_order_acquire))
        [[likely]] {
        // Successfully locked without contention.
        return;
    }

    vull::atomic_thread_fence(vull::memory_order_seq_cst);

    do {
        // State is either locked or locked with waiters at this point. Transition to locked with waiters if not
        // already.
        if (state == State::LockedWaiters ||
            m_state.cmpxchg(State::Locked, State::LockedWaiters, vull::memory_order_release,
                            vull::memory_order_relaxed) != State::Unlocked) {
            auto *current = Tasklet::current();
            if (!current->in_wait_list().load()) {
                // Add ourselves to the linked list of waiters.
                auto *waiter = m_wait_list.load();
                do {
                    current->set_linked_tasklet(waiter);
                } while (!m_wait_list.compare_exchange_weak(waiter, current, vull::memory_order_acq_rel,
                                                            vull::memory_order_acquire));

                // Fine for the window between adding ourselves to the wait list and setting the in wait list flag to
                // exist since the mutex owner will wait for us to stop running before attempting to unset the flag and
                // waking us.
                current->in_wait_list().store(true);

                // And yield to the scheduler.
                vull::yield();
            }
            vull::atomic_thread_fence(vull::memory_order_seq_cst);
        }
        // Woken up from yield, or mutex was just unlocked as we were trying to transition from locked to locked with
        // waiters. Try to cmpxchg from an expected unlocked state to a locked waiter state. We can't transition to just
        // a locked state since we can't know if any other tasklets are waiting.
        vull::atomic_thread_fence(vull::memory_order_seq_cst);
    } while ((state = m_state.cmpxchg(State::Unlocked, State::LockedWaiters, vull::memory_order_acq_rel,
                                      vull::memory_order_acquire)) != State::Unlocked);
    vull::atomic_thread_fence(vull::memory_order_seq_cst);
}

[[gnu::noinline]] void Mutex::unlock() {
    vull::atomic_thread_fence(vull::memory_order_seq_cst);

    Tasklet *to_wake = nullptr;
    //    if (m_state.load(vull::memory_order_acquire) == State::LockedWaiters) {
    // Try to pick a waiter to wake.
    to_wake = m_wait_list.load();
    Tasklet *desired;
    do {
        desired = to_wake != nullptr ? to_wake->linked_tasklet() : nullptr;
    } while (
        !m_wait_list.compare_exchange_weak(to_wake, desired, vull::memory_order_release, vull::memory_order_relaxed));
    //    }

    vull::atomic_thread_fence(vull::memory_order_seq_cst);
    if (to_wake != nullptr) {
        while (to_wake->state() == TaskletState::Running) {
            asm volatile("pause" ::: "memory");
        }
        VULL_ENSURE(to_wake->in_wait_list().load());
        to_wake->in_wait_list().store(false);
        to_wake->set_linked_tasklet(nullptr);
    }

    m_state.store(State::Unlocked, vull::memory_order_release);
    if (to_wake != nullptr) {
        vull::schedule(to_wake);
    }

    vull::atomic_thread_fence(vull::memory_order_seq_cst);
}

// void Mutex::lock() {
//     if (!m_locked.cmpxchg(false, true)) [[likely]] {
//         // Successfully locked without contention.
//         return;
//     }
//
//     do {
//         // Otherwies, increment the number of waiters.
//         m_wait_count.fetch_add(1);
//
//         // Add ourselves to the linked list of waiters.
//         auto *current = Tasklet::current();
//         auto *waiter = m_wait_list.load();
//         do {
//             current->set_linked_tasklet(waiter);
//         } while (!m_wait_list.compare_exchange(waiter, current));
//
//         // And yield to the scheduler.
//         vull::yield();
//     } while (m_locked.cmpxchg(false, true));
// }
//
// void Mutex::unlock() {
//     // Clear locked flag.
//     m_locked.store(false);
//
//     vull::atomic_thread_fence(vull::memory_order_seq_cst);
//
//     // Try to wake one waiter.
//     while (m_wait_count.load() != 0) {
//         Tasklet *to_wake = m_wait_list.load();
//         Tasklet *desired;
//         do {
//             desired = to_wake != nullptr ? to_wake->linked_tasklet() : nullptr;
//         } while (!m_wait_list.compare_exchange(to_wake, desired));
//
//         if (to_wake != nullptr) {
//             m_wait_count.fetch_sub(1);
//             while (to_wake->state() == TaskletState::Running) {
//             }
//             to_wake->set_linked_tasklet(nullptr);
//             vull::schedule(to_wake);
//             break;
//         }
//     }
// }

} // namespace vull
