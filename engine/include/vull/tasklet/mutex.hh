#pragma once

#include <vull/support/atomic.hh>

#include <stdint.h>

namespace vull {

class Tasklet;

class Mutex {
    Atomic<Tasklet *> m_wait_list;
    Atomic<uint16_t> m_wait_count;
    Atomic<bool> m_locked;

public:
    void lock();
    void unlock();

    bool locked() const { return m_locked.load(); }
};

} // namespace vull
