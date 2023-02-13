#pragma once

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
    };
    Vector<UniquePtr<Worker>> m_workers;

    static void *worker_entry(void *);

public:
    explicit Scheduler(uint32_t thread_count = 0);
    Scheduler(const Scheduler &) = delete;
    Scheduler(Scheduler &&) = delete;
    ~Scheduler();

    Scheduler &operator=(const Scheduler &) = delete;
    Scheduler &operator=(Scheduler &&) = delete;

    TaskletQueue &pick_victim(uint32_t &rng_state);

    bool start(Tasklet *tasklet);
    void stop();
};

} // namespace vull
