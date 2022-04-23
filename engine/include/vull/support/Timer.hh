#pragma once

#include <stdint.h>

namespace vull {

class Timer {
    uint64_t m_epoch{0};

public:
    Timer();

    float elapsed() const;
    void reset();
};

} // namespace vull
