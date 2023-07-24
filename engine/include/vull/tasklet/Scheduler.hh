#pragma once

#include <vull/container/Vector.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/support/Utility.hh>
#include <vull/tasklet/Tasklet.hh>

#include <pthread.h>
#include <stdint.h>

namespace vull {

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
    static Scheduler &current();

    explicit Scheduler(uint32_t thread_count = 0);
    Scheduler(const Scheduler &) = delete;
    Scheduler(Scheduler &&) = delete;
    ~Scheduler();

    Scheduler &operator=(const Scheduler &) = delete;
    Scheduler &operator=(Scheduler &&) = delete;

    TaskletQueue &pick_victim(uint32_t &rng_state);

    template <typename F>
    bool start(F &&callable);
    bool start(Tasklet *tasklet);
    void stop();
};

template <typename F>
bool Scheduler::start(F &&callable) {
    auto *tasklet = Tasklet::create_large();
    tasklet->set_callable(vull::forward<F>(callable));
    return start(tasklet);
}

} // namespace vull
