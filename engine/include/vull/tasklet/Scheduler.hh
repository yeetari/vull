#pragma once

#include <vull/support/Atomic.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/support/Vector.hh>

#include <pthread.h>
#include <stdint.h>

namespace vull {

class Tasklet;
class TaskletQueue;

class Scheduler {
    struct Worker {
        Scheduler &scheduler;
        UniquePtr<TaskletQueue> queue;
        pthread_t thread;
        uint32_t rng_state;
        Atomic<bool> running;
    };
    Vector<UniquePtr<Worker>> m_workers;

    Worker &pick_victim(uint32_t &rng_state);
    static void *thread_loop(void *);

public:
    explicit Scheduler(uint32_t thread_count = 0);
    Scheduler(const Scheduler &) = delete;
    Scheduler(Scheduler &&) = delete;
    ~Scheduler();

    Scheduler &operator=(const Scheduler &) = delete;
    Scheduler &operator=(Scheduler &&) = delete;

    bool start(Tasklet &&);
    void stop();
};

} // namespace vull
