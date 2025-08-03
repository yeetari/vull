#include <vull/tasklet/promise.hh>

#include <vull/platform/platform.hh>
#include <vull/support/assert.hh>
#include <vull/support/atomic.hh>
#include <vull/support/utility.hh>
#include <vull/tasklet/functions.hh>
#include <vull/tasklet/tasklet.hh>

#include <stdint.h>

namespace vull::tasklet {
namespace {

constexpr uint64_t k_has_threads_bit = 1uz << 62;
constexpr uint64_t k_fulfilled_bit = 1uz << 63;

Tasklet *to_tasklet(uint64_t state) {
    return vull::bit_cast<Tasklet *>(state & ~(0xfuz << 60));
}

} // namespace

uint32_t *PromiseBase::futex_word() {
    auto *pointer = vull::bit_cast<uint32_t *>(m_state.raw_ptr());
    if constexpr (platform::is_little_endian()) {
        pointer++;
    }
    return pointer;
}

void PromiseBase::wake_all() {
    // Atomically swap the state with the fulfilled sentinel bit.
    const auto state = m_state.exchange(k_fulfilled_bit, vull::memory_order_acq_rel);
    VULL_ASSERT((state & k_fulfilled_bit) == 0uz, "Promise fulfilled more than once");

    // Wake any waiting threads.
    if ((state & k_has_threads_bit) != 0uz) {
        platform::wake_address_all(futex_word());
    }

    // Dequeue and schedule waiting tasklets one-by-one.
    auto *tasklet = to_tasklet(state);
    while (tasklet != nullptr) {
        // Dequeue from the list before rescheduling the tasklet.
        auto *next = tasklet->pop_linked_tasklet();
        tasklet::schedule(vull::exchange(tasklet, next));
    }
}

bool PromiseBase::add_waiter(Tasklet *tasklet) {
    // TODO: relaxed might be fine here but wait() probably needs some ordering in the event that this function returns
    // false.
    uint64_t state = m_state.load(vull::memory_order_acquire);
    uint64_t desired;
    do {
        // Check if the promise has been fulfilled. If so, make sure to clear the linked tasklet.
        if ((state & k_fulfilled_bit) != 0uz) {
            tasklet->set_linked_tasklet(nullptr);
            return false;
        }

        // Point our tasklet to the current wait list head.
        tasklet->set_linked_tasklet(to_tasklet(state));

        // Build the desired state, preserving the threads waiting bit.
        desired = vull::bit_cast<uint64_t>(tasklet) | (state & (0xfuz << 60));
        VULL_ASSERT(to_tasklet(desired) == tasklet);
    } while (!m_state.compare_exchange_weak(state, desired, vull::memory_order_acq_rel, vull::memory_order_acquire));
    return true;
}

bool PromiseBase::is_fulfilled() const {
    return (m_state.load(vull::memory_order_relaxed) & k_fulfilled_bit) != 0uz;
}

void PromiseBase::reset() {
    VULL_ASSERT(is_fulfilled());
    m_state.store(0uz, vull::memory_order_release);
}

void PromiseBase::wake_on_fulfillment(Tasklet *tasklet) {
    if (!add_waiter(tasklet)) {
        // Promise already fulfilled - schedule the tasklet immediately.
        tasklet::schedule(tasklet);
    }
}

void PromiseBase::wait() {
    if (!tasklet::in_tasklet_context()) {
        uint64_t state = m_state.load(vull::memory_order_acquire);
        while ((state & k_fulfilled_bit) == 0uz) {
            // Set the has threads bit. It doesn't really matter if this races with a fulfillment and ends up setting
            // the bit after the fulfilled bit has been set.
            m_state.fetch_or(k_has_threads_bit, vull::memory_order_release);

            // Wait on the upper half of the state word.
            const auto expected_word = static_cast<uint32_t>((state | k_has_threads_bit) >> 32);
            platform::wait_address(futex_word(), expected_word);
            state = m_state.load(vull::memory_order_acquire);
        }
    } else if (add_waiter(Tasklet::current())) {
        // Promise not yet fulfilled - suspend ourselves.
        tasklet::suspend();
    }
}

} // namespace vull::tasklet
