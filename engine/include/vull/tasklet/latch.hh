#pragma once

#include <vull/support/atomic.hh>
#include <vull/tasklet/promise.hh>

#include <stdint.h>

namespace vull {

class Latch {
    Promise<void> m_promise;
    Atomic<uint32_t> m_value;

public:
    explicit Latch(uint32_t value) : m_value(value) {}
    Latch(const Latch &) = delete;
    Latch(Latch &&) = delete;
    ~Latch() = default;

    Latch &operator=(const Latch &) = delete;
    Latch &operator=(Latch &&) = delete;

    void count_down(uint32_t by = 1);
    bool try_wait() const;
    void wait();
};

} // namespace vull
