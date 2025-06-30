#pragma once

#include <vull/support/atomic.hh>

#include <stdint.h>

namespace vull::platform {

class Semaphore {
    Atomic<uint64_t> m_data;

public:
    Semaphore(uint32_t value = 0) : m_data(value) {}
    Semaphore(const Semaphore &) = delete;
    Semaphore(Semaphore &&) = delete;
    ~Semaphore() = default;

    Semaphore &operator=(const Semaphore &) = delete;
    Semaphore &operator=(Semaphore &&) = delete;

    void post();
    void release();
    bool try_wait();
    void wait();
};

} // namespace vull::platform
