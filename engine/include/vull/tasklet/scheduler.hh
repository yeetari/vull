#pragma once

#include <vull/container/vector.hh>
#include <vull/platform/system_semaphore.hh>
#include <vull/support/atomic.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>
#include <vull/tasklet/tasklet.hh>

#include <pthread.h>
#include <stdint.h>

namespace vull::tasklet {

class TaskletQueue;

class Scheduler {
    Vector<pthread_t> m_workers;
    UniquePtr<TaskletQueue> m_queue;
    SystemSemaphore m_work_available;
    Atomic<bool> m_running;

    static void *worker_entry(void *);

public:
    static Scheduler &current();

    explicit Scheduler(uint32_t thread_count = 0);
    Scheduler(const Scheduler &) = delete;
    Scheduler(Scheduler &&) = delete;
    ~Scheduler();

    Scheduler &operator=(const Scheduler &) = delete;
    Scheduler &operator=(Scheduler &&) = delete;

    void join();
    template <typename F>
    bool start(F &&callable);
    bool start(Tasklet *tasklet);
    void stop();

    uint32_t tasklet_count() const;
    bool is_running() const { return m_running.load(vull::memory_order_acquire); }
};

template <typename F>
bool Scheduler::start(F &&callable) {
    auto *tasklet = Tasklet::create<TaskletSize::Large>();
    tasklet->set_callable([this, callable = vull::move(callable)] {
        callable();
        stop();
    });
    return start(tasklet);
}

} // namespace vull::tasklet
