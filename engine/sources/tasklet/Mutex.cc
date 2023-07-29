#include <vull/tasklet/Mutex.hh>

#include <vull/support/Atomic.hh>
#include <vull/tasklet/Tasklet.hh>

namespace vull {

void Mutex::lock() {
    if (!m_locked.cmpxchg(false, true)) [[likely]] {
        // Successfully locked without contention.
        return;
    }

    do {
        // Otherwies, increment the number of waiters.
        m_wait_count.fetch_add(1);

        // Add ourselves to the linked list of waiters.
        auto *current = Tasklet::current();
        auto *waiter = m_wait_list.load();
        do {
            current->set_linked_tasklet(waiter);
        } while (!m_wait_list.compare_exchange_weak(waiter, current));

        // And yield to the scheduler.
        vull::yield();
    } while (m_locked.cmpxchg_weak(false, true));
}

void Mutex::unlock() {
    // Clear locked flag.
    m_locked.store(false);

    // Try to wake one waiter.
    while (m_wait_count.load() != 0) {
        Tasklet *to_wake = m_wait_list.load();
        Tasklet *desired;
        do {
            desired = to_wake != nullptr ? to_wake->linked_tasklet() : nullptr;
        } while (!m_wait_list.compare_exchange_weak(to_wake, desired));

        if (to_wake != nullptr) {
            m_wait_count.fetch_sub(1);
            while (to_wake->state() == TaskletState::Running) {
            }
            to_wake->set_linked_tasklet(nullptr);
            vull::schedule(to_wake);
            break;
        }
    }
}

} // namespace vull
