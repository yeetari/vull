#pragma once

#include <vull/support/Atomic.hh>

namespace vull {

class Semaphore {
    Atomic<bool> m_acquired;

public:
    bool try_acquire();
    void release();
};

inline bool Semaphore::try_acquire() {
    return !m_acquired.exchange(true, MemoryOrder::Acquire);
}

inline void Semaphore::release() {
    m_acquired.store(false, MemoryOrder::Release);
}

} // namespace vull
