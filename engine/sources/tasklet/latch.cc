#include <vull/tasklet/latch.hh>

#include <vull/support/assert.hh>
#include <vull/support/atomic.hh>
#include <vull/tasklet/promise.hh>

namespace vull::tasklet {

void Latch::count_down(uint32_t by) {
    const auto value = m_value.fetch_sub(by, vull::memory_order_acq_rel);
    if (value != by) {
        VULL_ASSERT(value > by);
        return;
    }

    // Wake all of the waiters.
    m_promise.fulfill();
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
