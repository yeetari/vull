#pragma once

#include <vull/support/atomic.hh>

#include <stdint.h>

namespace vull {

class SystemMutex {
    enum class State : uint32_t {
        Unlocked,
        Locked,
        LockedWaiters,
    };
    Atomic<State> m_state{State::Unlocked};

public:
    SystemMutex() = default;
    SystemMutex(const SystemMutex &) = delete;
    SystemMutex(SystemMutex &&) = delete;
    ~SystemMutex() = default;

    SystemMutex &operator=(const SystemMutex &) = delete;
    SystemMutex &operator=(SystemMutex &&) = delete;

    void lock();
    void unlock();
};

} // namespace vull
