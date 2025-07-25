#pragma once

#include <vull/support/atomic.hh>

namespace vull::tasklet {

class Tasklet;

/**
 * @brief A tasklet-aware mutual exclusion synchronisation primitive providing non-recursive exclusive ownership.
 *
 * A mutex can be acquired by a tasklet by either calling the blocking `lock()` function, or by successfully calling
 * `try_lock()`. Once acquired, the mutex is considered owned by the calling tasklet.
 *
 * Whilst a mutex is owned, any other tasklet attempting to acquire it will either suspend if `lock()` is called, or
 * receive false from `try_lock()` until the owning tasklet releases the mutex by calling `unlock()`.
 *
 * This mutex is non-recursive; an owning tasklet must not attempt to acquire the mutex twice.
 *
 * @ingroup Tasklet
 */
class Mutex {
    Atomic<Tasklet *> m_wait_list;

public:
    Mutex();
    Mutex(const Mutex &) = delete;
    Mutex(Mutex &&) = delete;
    ~Mutex();

    Mutex &operator=(const Mutex &) = delete;
    Mutex &operator=(Mutex &&) = delete;

    /**
     * @brief Attempts to acquire the mutex without suspending the calling tasklet.
     *
     * @return true if the mutex was successfully acquired; false otherwise
     */
    bool try_lock();

    /**
     * @brief Suspends the calling tasklet until ownership of the mutex is acquired. Upon return, the mutex is
     * exclusively owned by the calling tasklet.
     */
    void lock();

    /**
     * @brief Releases ownership of the mutex.
     *
     * The behaviour of this function is undefined if the mutex is not currently locked or is not owned by the
     * calling tasklet.
     */
    void unlock();
};

} // namespace vull::tasklet
