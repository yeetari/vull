#pragma once

#include <vull/support/atomic.hh>
#include <vull/tasklet/promise.hh>

#include <stdint.h>

namespace vull::tasklet {

/**
 * @brief A downwards counter which can be used to synchronise tasklet execution. Tasklets can wait on the latch to
 * reach a zero count.
 *
 * @ingroup Tasklet
 */
class Latch {
    Promise<void> m_promise;
    Atomic<uint32_t> m_value;

public:
    /**
     * @brief Constructs a latch with the given expected number of arrivals.
     */
    explicit Latch(uint32_t expected) : m_value(expected) {}
    Latch(const Latch &) = delete;
    Latch(Latch &&) = delete;
    ~Latch() = default;

    Latch &operator=(const Latch &) = delete;
    Latch &operator=(Latch &&) = delete;

    /**
     * @brief Decrements the count and, if necessary, suspends the calling tasklet until the count is zero.
     *
     * The behaviour is undefined if update is greater than the remaining count.
     *
     * @param update the amount to decrement the counter by
     */
    void arrive(uint32_t update = 1);

    /**
     * @brief Decrements the count without suspending the calling tasklet.
     *
     * The behaviour is undefined if update is greater than the remaining count.
     *
     * @param update the amount to decrement the counter by
     */
    void count_down(uint32_t update = 1);

    /**
     * @brief Tests if the latch has reached a zero count.
     *
     * @return true if the count is zero; false otherwise
     */
    bool try_wait() const;

    /**
     * @brief Suspends the calling tasklet until the count is zero. Returns immediately if zero already.
     */
    void wait();
};

} // namespace vull::tasklet
