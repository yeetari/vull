#pragma once

#include <vull/support/Atomic.hh>

#include <stdint.h>

namespace vull {

class Mutex {
    enum class State : uint32_t {
        Unlocked,
        Locked,
        LockedWaiters,
    };
    Atomic<State> m_state{State::Unlocked};

public:
    Mutex() = default;
    Mutex(const Mutex &) = delete;
    Mutex(Mutex &&) = delete;
    ~Mutex() = default;

    Mutex &operator=(const Mutex &) = delete;
    Mutex &operator=(Mutex &&) = delete;

    void lock();
    void unlock();
};

} // namespace vull
