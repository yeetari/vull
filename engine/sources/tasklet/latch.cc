#include <vull/tasklet/latch.hh>

#include <vull/support/assert.hh>
#include <vull/support/atomic.hh>
#include <vull/tasklet/promise.hh>

namespace vull::tasklet {

void Latch::arrive(uint32_t update) {
    count_down(update);
    wait();
}

void Latch::count_down(uint32_t update) {
    const auto value = m_value.fetch_sub(update, vull::memory_order_release);
    if (value == update) {
        // Wake all of the waiters.
        m_promise.fulfill();
    } else {
        VULL_ASSERT(value > update);
    }
}

bool Latch::try_wait() const {
    return m_value.load(vull::memory_order_acquire) == 0;
}

void Latch::wait() {
    // Wait on the promise if the latch hasn't been set yet.
    if (!try_wait()) {
        m_promise.wait();
    }
    VULL_ASSERT(m_value.load(vull::memory_order_relaxed) == 0);
}

} // namespace vull::tasklet
