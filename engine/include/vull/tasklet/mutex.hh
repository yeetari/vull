#pragma once

#include <vull/support/atomic.hh>

namespace vull::tasklet {

class Tasklet;

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
     * @brief Attempts to lock the mutex in a non-blocking way.
     *
     * @return true if the mutex was successfully locked; false otherwise
     */
    bool try_lock();

    /**
     * @brief Attempts to lock the mutex and blocks until ownership of the mutex is taken.
     */
    void lock();

    /**
     * @brief Unlocks the mutex.
     *
     * The behaviour of this function is undefined if the mutex is not locked or if the mutex is not owned by the
     * current tasklet.
     */
    void unlock();
};

} // namespace vull::tasklet
