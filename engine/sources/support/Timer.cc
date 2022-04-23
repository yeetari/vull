#include <vull/support/Timer.hh>

#include <time.h>

namespace vull {
namespace {

uint64_t monotonic_time() {
    struct timespec ts {};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return uint64_t(ts.tv_sec) * 1000000000ull + uint64_t(ts.tv_nsec);
}

} // namespace

Timer::Timer() : m_epoch(monotonic_time()) {}

float Timer::elapsed() const {
    return static_cast<float>(monotonic_time() - m_epoch) / 1000000000.0f;
}

void Timer::reset() {
    m_epoch = monotonic_time();
}

} // namespace vull
