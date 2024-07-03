#pragma once

#include <vull/support/atomic.hh>

#include <stdint.h>

namespace vull {

class Tasklet;

class Mutex {
    enum class State : uint32_t {
        Unlocked,
        Locked,
        LockedWaiters,
    };
    Atomic<Tasklet *> m_wait_list;
    Atomic<State> m_state{State::Unlocked};

//    Atomic<uint32_t> m_wait_count;
//    Atomic<bool> m_locked;

public:
    void lock();
    void unlock();

//    bool locked() const { return m_locked.load(); }
    bool locked() const { return m_state.load(vull::memory_order_acquire) != State::Unlocked; }
};

} // namespace vull
