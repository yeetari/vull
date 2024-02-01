#pragma once

#include <vull/support/atomic.hh>

#include <stdint.h>

namespace vull {

class Tasklet;

class Latch {
    Atomic<Tasklet *> m_wait_list;
    Atomic<uint32_t> m_value;

public:
    explicit Latch(uint32_t value) : m_value(value) {}

    void count_down(uint32_t by = 1);
    bool try_wait() const;
    void wait();
};

} // namespace vull
