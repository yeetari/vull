#include <vull/tasklet/Latch.hh>

#include <vull/support/Assert.hh>
#include <vull/support/Atomic.hh>
#include <vull/tasklet/Tasklet.hh>

namespace vull {

void Latch::count_down(uint32_t by) {
    const auto value = m_value.fetch_sub(by);
    if (value != by) {
        VULL_ASSERT(value > by);
        return;
    }

    // Wake all waiters.
    while (auto *to_wake = m_wait_list.load()) {
        Tasklet *desired;
        do {
            desired = to_wake != nullptr ? to_wake->linked_tasklet() : nullptr;
        } while (!m_wait_list.compare_exchange_weak(to_wake, desired));

        while (to_wake->state() == TaskletState::Running) {
        }
        to_wake->set_linked_tasklet(nullptr);
        vull::schedule(to_wake);
    }
}

bool Latch::try_wait() const {
    return m_value.load() == 0;
}

void Latch::wait() {
    if (try_wait()) {
        // Already zero.
        return;
    }

    // Otherwise add ourselves to the linked list of waiters.
    auto *current = Tasklet::current();
    auto *waiter = m_wait_list.load();
    do {
        current->set_linked_tasklet(waiter);
    } while (!m_wait_list.compare_exchange_weak(waiter, current));

    // And yield to the scheduler.
    vull::yield();
    VULL_ASSERT(try_wait());
}

} // namespace vull
