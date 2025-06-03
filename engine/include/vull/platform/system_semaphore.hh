#pragma once

#include <vull/support/atomic.hh>

#include <stdint.h>

namespace vull {

class SystemSemaphore {
    Atomic<uint64_t> m_data{1};

public:
    SystemSemaphore() = default;
    SystemSemaphore(const SystemSemaphore &) = delete;
    SystemSemaphore(SystemSemaphore &&) = delete;
    ~SystemSemaphore() = default;

    SystemSemaphore &operator=(const SystemSemaphore &) = delete;
    SystemSemaphore &operator=(SystemSemaphore &&) = delete;

    void post();
    bool try_wait();
    void wait();
};

} // namespace vull
