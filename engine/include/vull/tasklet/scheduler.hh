#pragma once

#include <vull/container/vector.hh>
#include <vull/platform/system_semaphore.hh>
#include <vull/support/assert.hh>
#include <vull/support/atomic.hh>
#include <vull/support/function.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>
#include <vull/tasklet/promise.hh>
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
    template <TaskletSize Size = TaskletSize::Normal, typename F>
    auto run(F &&callable);
    bool start(Tasklet *tasklet);

    uint32_t tasklet_count() const;
    bool is_running() const { return m_running.load(vull::memory_order_acquire); }
};

template <TaskletSize Size, typename F>
auto Scheduler::run(F &&callable) {
    using R = FunctionTraits<F>::result_type;
    Promise<R> promise;
    SystemSemaphore semaphore;
    auto *tasklet = Tasklet::create<Size>();
    tasklet->set_callable([&, callable = vull::move(callable)] {
        if constexpr (vull::is_same<R, void>) {
            callable();
            promise.fulfill();
        } else {
            promise.fulfill(callable());
        }
        semaphore.post();
    });
    VULL_ENSURE(start(tasklet));
    semaphore.wait();
    VULL_ASSERT(promise.is_fulfilled());
    if constexpr (!vull::is_same<R, void>) {
        return vull::move(promise.value());
    }
}

} // namespace vull::tasklet
