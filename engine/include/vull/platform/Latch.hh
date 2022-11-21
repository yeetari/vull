#pragma once

#include <vull/support/Atomic.hh>

#include <stdint.h>

namespace vull {

class Latch {
    Atomic<uint32_t> m_value{1};

public:
    Latch() = default;
    Latch(const Latch &) = delete;
    Latch(Latch &&) = delete;
    ~Latch() = default;

    Latch &operator=(const Latch &) = delete;
    Latch &operator=(Latch &&) = delete;

    void count_down();
    void increment(uint32_t amt = 1) { m_value.fetch_add(amt, MemoryOrder::AcqRel); }
    void wait();
};

} // namespace vull
