#pragma once

#include <stdint.h>

namespace vull::platform {

class Timer {
    uint64_t m_epoch{0};

public:
    Timer();

    float elapsed() const;
    uint64_t elapsed_ns() const;
    void reset();
};

} // namespace vull::platform
