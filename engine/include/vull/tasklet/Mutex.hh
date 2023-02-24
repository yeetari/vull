#pragma once

#include <vull/support/Atomic.hh>

namespace vull {

class Tasklet;

class Mutex {
    Atomic<Tasklet *> m_waiter;
    Atomic<bool> m_locked;

public:
    void lock();
    void unlock();

    bool locked() const { return m_locked.load(); }
};

} // namespace vull
