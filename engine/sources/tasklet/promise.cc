#include <vull/tasklet/promise.hh>

#include <vull/support/atomic.hh>
#include <vull/support/utility.hh>
#include <vull/tasklet/functions.hh>
#include <vull/tasklet/tasklet.hh>

namespace vull::tasklet {

#define FULFILLED_SENTINEL vull::bit_cast<Tasklet *>(-1ull)

void PromiseBase::wake_all() {
    // Atomically swap list with the fulfilled sentinel.
    auto *tasklet = m_wait_list.exchange(FULFILLED_SENTINEL, vull::memory_order_acquire);
    while (tasklet != nullptr) {
        // Dequeue from the list before rescheduling the tasklet.
        auto *next = tasklet->pop_linked_tasklet();
        tasklet::schedule(vull::exchange(tasklet, next));
    }
}

bool PromiseBase::add_waiter(Tasklet *tasklet) {
    auto *head = m_wait_list.load(vull::memory_order_relaxed);
    do {
        if (head == FULFILLED_SENTINEL) {
            // Promise was fulfilled. Make sure to clear the linked tasklet.
            tasklet->set_linked_tasklet(nullptr);
            return false;
        }
        tasklet->set_linked_tasklet(head);
    } while (!m_wait_list.compare_exchange_weak(head, tasklet, vull::memory_order_release));
    return true;
}

bool PromiseBase::is_fulfilled() const {
    return m_wait_list.load(vull::memory_order_relaxed) == FULFILLED_SENTINEL;
}

void PromiseBase::wake_on_fulfillment(Tasklet *tasklet) {
    if (!add_waiter(tasklet)) {
        // Promise already fulfilled - schedule the tasklet immediately.
        tasklet::schedule(tasklet);
    }
}

void PromiseBase::wait() {
    if (add_waiter(Tasklet::current())) {
        // Promise not yet fulfilled - suspend ourselves.
        tasklet::suspend();
    }
}

} // namespace vull::tasklet
