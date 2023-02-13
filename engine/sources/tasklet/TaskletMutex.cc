#include <vull/tasklet/TaskletMutex.hh>

#include <vull/support/Atomic.hh>
#include <vull/tasklet/Tasklet.hh>

namespace vull {

void TaskletMutex::lock() {
    if (!m_locked.cmpxchg(false, true)) [[likely]] {
        // Successfully locked without contention.
        return;
    }

    do {
        // Otherwise add ourselves to the linked list of waiters.
        auto *current = Tasklet::current();
        auto *waiter = m_waiter.load();
        do {
            current->set_linked_tasklet(waiter);
        } while (!m_waiter.compare_exchange_weak(waiter, current));

        // And yield to the scheduler.
        vull::yield();
    } while (m_locked.cmpxchg_weak(false, true));
}

void TaskletMutex::unlock() {
    // Dequeue one waiter from the linked list.
    auto *to_wake = m_waiter.load();
    Tasklet *desired;
    do {
        desired = to_wake != nullptr ? to_wake->linked_tasklet() : nullptr;
    } while (!m_waiter.compare_exchange_weak(to_wake, desired));

    // Clear locked flag.
    m_locked.store(false);

    // Wake one waiter.
    if (to_wake != nullptr) {
        to_wake->set_linked_tasklet(nullptr);
        vull::schedule(to_wake);
    }
}

} // namespace vull
