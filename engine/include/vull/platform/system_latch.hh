#pragma once

#include <vull/support/atomic.hh>

#include <stdint.h>

namespace vull {

class SystemLatch {
    Atomic<uint32_t> m_value{1};

public:
    SystemLatch() = default;
    SystemLatch(const SystemLatch &) = delete;
    SystemLatch(SystemLatch &&) = delete;
    ~SystemLatch() = default;

    SystemLatch &operator=(const SystemLatch &) = delete;
    SystemLatch &operator=(SystemLatch &&) = delete;

    void count_down();
    void increment(uint32_t amt = 1) { m_value.fetch_add(amt, vull::memory_order_acq_rel); }
    void wait();
};

} // namespace vull
