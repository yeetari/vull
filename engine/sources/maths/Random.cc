#include <vull/maths/Random.hh>

#include <vull/support/Array.hh>
#include <vull/support/Utility.hh>

#include <stdint.h>

namespace vull {

VULL_GLOBAL(static thread_local Array<uint32_t, 6> s_state{123456789, 362436069, 521288629, 88675123, 5783321,
                                                           6615241});

// xorwow algorithm.
uint32_t fast_rand() {
    uint32_t t = s_state[0] ^ (s_state[0] >> 2u);
    s_state[0] = s_state[1];
    s_state[1] = s_state[2];
    s_state[2] = s_state[3];
    s_state[3] = s_state[4];
    s_state[4] = (s_state[4] ^ (s_state[4] << 4u)) ^ (t ^ (t << 1u));
    return s_state[4] + (s_state[5] += 362437);
}

void seed_rand(uint32_t seed) {
    s_state[0] = seed;
}

} // namespace vull
