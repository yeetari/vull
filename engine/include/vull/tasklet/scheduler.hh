#pragma once

#include <vull/container/vector.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>
#include <vull/tasklet/tasklet.hh>

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
    tasklet->set_callable([this, callable = vull::move(callable)] {
        callable();
        stop();
    });
    return start(tasklet);
}

} // namespace vull
