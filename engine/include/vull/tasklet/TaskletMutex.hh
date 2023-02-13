#pragma once

#include <vull/support/Atomic.hh>

namespace vull {

class Tasklet;

class TaskletMutex {
    Atomic<bool> m_locked;
    Atomic<Tasklet *> m_waiter;

public:
    void lock();
    void unlock();
};

} // namespace vull
