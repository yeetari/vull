#include <vull/tasklet/mutex.hh>

#include <vull/support/assert.hh>
#include <vull/support/atomic.hh>
#include <vull/support/utility.hh>
#include <vull/tasklet/functions.hh>
#include <vull/tasklet/tasklet.hh>

// TODO: Add locked without waiters optimisation.
// TODO: Only wake one waiter upon unlock to avoid thundering herd. This is hard as unlocking the mutex would need to
//       signal the unlock and keep the remaining tasklets in the wait list.

namespace vull::tasklet {

#define UNLOCKED_SENTINEL vull::bit_cast<Tasklet *>(-1uz)

Mutex::Mutex() {
    // Start in unlocked state.
    m_wait_list.store(UNLOCKED_SENTINEL, vull::memory_order_release);
}

Mutex::~Mutex() {
    // Verify mutex ends in unlocked state.
    VULL_ASSERT(m_wait_list.load(vull::memory_order_relaxed) == UNLOCKED_SENTINEL);
}

bool Mutex::try_lock() {
    auto *expected = UNLOCKED_SENTINEL;
    return m_wait_list.compare_exchange(expected, nullptr, vull::memory_order_acquire);
}

void Mutex::lock() {
    while (true) {
        auto *tasklet = UNLOCKED_SENTINEL;
        if (m_wait_list.compare_exchange(tasklet, nullptr, vull::memory_order_acquire)) {
            // We have successfully locked the mutex.
            return;
        }

        // TODO: Implement locking for normal threads via futex.
        if (!tasklet::in_tasklet_context()) {
            continue;
        }

        // Otherwise the mutex is locked and we need to add ourselves to the wait list.
        auto *current = Tasklet::current();
        current->set_linked_tasklet(tasklet);
        if (m_wait_list.compare_exchange(tasklet, current, vull::memory_order_acq_rel)) {
            // Successfully added ourselves to the wait list - suspend until the mutex is unlocked.
            tasklet::suspend();
        } else {
            // Compare exchange failed - either the mutex was unlocked or we failed a race with another tasklet to enter
            // the wait list. In either case, go back to the top of the loop.
            current->set_linked_tasklet(nullptr);
            vull::atomic_thread_fence(vull::memory_order_release);
        }
    }
}

void Mutex::unlock() {
    // Atomically swap the list of waiters with the unlocked sentinel.
    auto *tasklet = m_wait_list.exchange(UNLOCKED_SENTINEL, vull::memory_order_acq_rel);

    // Verify that the mutex wasn't already unlocked.
    VULL_ASSERT(tasklet != UNLOCKED_SENTINEL);

    // Wake all of the waiters.
    while (tasklet != nullptr) {
        // Dequeue from the list before rescheduling the tasklet.
        auto *next = tasklet->pop_linked_tasklet();
        tasklet::schedule(vull::exchange(tasklet, next));
    }
}

} // namespace vull::tasklet
