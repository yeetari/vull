#pragma once

#include <vull/support/atomic.hh>

#include <stdint.h>

namespace vull {

class SystemSemaphore {
    Atomic<uint64_t> m_data;

public:
    SystemSemaphore(uint32_t value = 0) : m_data(value) {}
    SystemSemaphore(const SystemSemaphore &) = delete;
    SystemSemaphore(SystemSemaphore &&) = delete;
    ~SystemSemaphore() = default;

    SystemSemaphore &operator=(const SystemSemaphore &) = delete;
    SystemSemaphore &operator=(SystemSemaphore &&) = delete;

    void post();
    void release();
    bool try_wait();
    void wait();
};

} // namespace vull
