#pragma once

#include <vull/container/mpmc_queue.hh>
#include <vull/container/vector.hh>
#include <vull/platform/event.hh>
#include <vull/platform/semaphore.hh>
#include <vull/platform/thread.hh>
#include <vull/support/assert.hh>
#include <vull/support/atomic.hh>
#include <vull/support/function.hh>
#include <vull/support/shared_ptr.hh> // IWYU pragma: keep
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>
#include <vull/tasklet/promise.hh>
#include <vull/tasklet/tasklet.hh>

#include <stdint.h>

namespace vull::tasklet {

class IoRequest;
class TaskletQueue;

struct IoQueue : MpmcQueue<IoRequest *, 11> {
    platform::Event quit_event;
    platform::Event submit_event;
    Atomic<uint32_t> pending;
};

class Scheduler {
    Vector<platform::Thread> m_worker_threads;
    UniquePtr<TaskletQueue> m_queue;
    UniquePtr<IoQueue> m_io_queue;
    platform::Semaphore m_work_available{1};
    platform::Thread m_io_thread;
    Atomic<uint32_t> m_alive_worker_count;
    Atomic<bool> m_running;

public:
    static Scheduler &current();

    explicit Scheduler(uint32_t thread_count = 0);
    Scheduler(const Scheduler &) = delete;
    Scheduler(Scheduler &&) = delete;
    ~Scheduler();

    Scheduler &operator=(const Scheduler &) = delete;
    Scheduler &operator=(Scheduler &&) = delete;

    void decrease_worker_count() { m_alive_worker_count.fetch_sub(1); }
    void join();
    template <TaskletSize Size = TaskletSize::Normal, typename F>
    auto run(F &&callable);
    bool start(Tasklet *tasklet);
    void setup_thread();
    void submit_io_request(SharedPtr<IoRequest> request);

    uint32_t tasklet_count() const;
    bool is_running() const { return m_running.load(vull::memory_order_acquire); }
};

template <TaskletSize Size, typename F>
auto Scheduler::run(F &&callable) {
    using R = FunctionTraits<F>::result_type;
    Promise<R> promise;
    platform::Semaphore semaphore;
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
