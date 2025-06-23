#include <vull/tasklet/scheduler.hh>

#include <vull/container/array.hh>
#include <vull/container/mpmc_queue.hh>
#include <vull/container/vector.hh>
#include <vull/core/log.hh>
#include <vull/maths/common.hh>
#include <vull/platform/system_semaphore.hh>
#include <vull/support/assert.hh>
#include <vull/support/atomic.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>
#include <vull/tasklet/functions.hh>
#include <vull/tasklet/tasklet.hh>

#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

namespace vull::tasklet {

extern "C" void vull_make_context(void *stack_top, void (*entry_point)(Tasklet *));
extern "C" [[noreturn]] void vull_load_context(Tasklet *tasklet, Tasklet *to_free);
extern "C" void vull_swap_context(Tasklet *from, Tasklet *to);

class TaskletQueue : public MpmcQueue<Tasklet *, 11> {};

VULL_GLOBAL(static thread_local Tasklet *s_current_tasklet = nullptr);
VULL_GLOBAL(static thread_local Tasklet *s_scheduler_tasklet = nullptr);
VULL_GLOBAL(static thread_local Tasklet *s_to_schedule = nullptr);
VULL_GLOBAL(static thread_local TaskletQueue *s_queue = nullptr);
VULL_GLOBAL(static thread_local Scheduler *s_scheduler = nullptr);
VULL_GLOBAL(static thread_local SystemSemaphore *s_work_available = nullptr);

Tasklet *Tasklet::current() {
    return s_current_tasklet;
}

Scheduler &Scheduler::current() {
    return *s_scheduler;
}

Scheduler::Scheduler(uint32_t thread_count) {
    if (thread_count == 0) {
        thread_count = vull::max(static_cast<uint32_t>(sysconf(_SC_NPROCESSORS_ONLN)) / 2, 2);
    }
    m_workers.ensure_size(thread_count);
    m_queue = vull::make_unique<TaskletQueue>();
}

Scheduler::~Scheduler() {
    join();
}

void Scheduler::join() {
    m_running.store(false, vull::memory_order_release);
    m_work_available.release();
    for (auto &worker : m_workers) {
        pthread_join(worker, nullptr);
    }
    m_workers.clear();
}

bool Scheduler::start(Tasklet *tasklet) {
    if (m_workers.empty()) {
        return false;
    }
    if (!m_queue->enqueue(tasklet)) {
        return false;
    }
    m_running.store(true, vull::memory_order_release);
    for (auto &worker : m_workers) {
        if (pthread_create(&worker, nullptr, &worker_entry, this) != 0) {
            return false;
        }
    }

    const auto core_count = static_cast<uint32_t>(sysconf(_SC_NPROCESSORS_ONLN));
    if (m_workers.size() <= core_count) {
        for (uint32_t i = 0; i < m_workers.size(); i++) {
            cpu_set_t set;
            CPU_ZERO(&set);
            CPU_SET(i, &set);
            pthread_setaffinity_np(m_workers[i], sizeof(cpu_set_t), &set);
        }
    }
    vull::info("[tasklet] Created {} threads", m_workers.size());
    return true;
}

uint32_t Scheduler::tasklet_count() const {
    return m_queue->size();
}

static Tasklet *pick_next() {
    auto *next = vull::exchange(s_to_schedule, nullptr);
    while (next == nullptr) {
        s_work_available->wait();
        if (!s_scheduler->is_running() && s_queue->empty()) {
            pthread_exit(nullptr);
        }

        next = s_queue->dequeue();
        if (next == nullptr) {
            s_work_available->post();
        }
    }

    if (next->state() == TaskletState::Uninitialised) {
        vull_make_context(next->stack_top(), next->invoker());
    } else {
        // The tasklet may still be in the running state if it is in the middle yielding, spin whilst that's the case.
        // TODO: Can we do something better here?
        while (next->state() == TaskletState::Running) {
        }
        VULL_ASSERT(next->state() == TaskletState::Waiting);
    }
    next->set_state(TaskletState::Running);
    return next;
}

[[noreturn]] void tasklet_switch_next(Tasklet *current) {
    auto *next = pick_next();
    vull_load_context(s_current_tasklet = next, current);
}

[[noreturn]] static void scheduler_fn(Tasklet *) {
    // s_current_tasklet should still be the tasklet we yielded from, or nullptr in the edge case that this is the first
    // schedule.
    VULL_ASSERT(s_current_tasklet != s_scheduler_tasklet);
    VULL_ASSERT(s_current_tasklet->state() != TaskletState::Done);
    s_current_tasklet->set_state(TaskletState::Waiting);

    auto *next = pick_next();
    vull_load_context(s_current_tasklet = next, nullptr);
}

[[noreturn]] static void segfault_handler(int, siginfo_t *info, void *) {
    const auto address = reinterpret_cast<uintptr_t>(info->si_addr);
    const auto *tasklet = static_cast<void *>(Tasklet::current());
    if (Tasklet::current()->is_guard_page(address)) {
        fprintf(stderr, "Stack overflow in tasklet %p\n", tasklet);
    } else {
        fprintf(stderr, "Segfault at address 0x%lx in tasklet %p\n", address, tasklet);
    }
    abort();
}

void *Scheduler::worker_entry(void *scheduler_ptr) {
    auto &scheduler = *static_cast<Scheduler *>(scheduler_ptr);
    s_scheduler = &scheduler;
    s_queue = scheduler.m_queue.ptr();
    s_work_available = &scheduler.m_work_available;

    sigset_t sig_set;
    sigfillset(&sig_set);
    sigdelset(&sig_set, SIGSEGV);
    if (pthread_sigmask(SIG_BLOCK, &sig_set, nullptr) != 0) {
        vull::error("[tasklet] Failed to mask signals");
    }

    struct sigaction sa{
        .sa_flags = SA_SIGINFO,
    };
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = &segfault_handler;
    sigaction(SIGSEGV, &sa, nullptr);

    // Use thread stack for scheduler tasklet.
    Array<uint8_t, 131072> tasklet_data{};
    s_scheduler_tasklet = new (tasklet_data.data()) Tasklet(tasklet_data.size(), nullptr);
    vull_make_context(s_scheduler_tasklet->stack_top(), scheduler_fn);

    s_work_available->post();
    auto *next = pick_next();
    vull_load_context(s_current_tasklet = next, nullptr);
}

void schedule(Tasklet *tasklet) {
    VULL_ASSERT_PEDANTIC(s_queue != nullptr);
    s_work_available->post();
    while (!s_queue->enqueue(tasklet)) {
        yield();
    }
}

void suspend() {
    VULL_ASSERT(s_current_tasklet->state() == TaskletState::Running);
    vull_swap_context(s_current_tasklet, s_scheduler_tasklet);
}

void yield() {
    auto *dequeued = s_queue->dequeue();
    if (dequeued == nullptr) {
        return;
    }
    [[maybe_unused]] bool success = s_queue->enqueue(s_current_tasklet);
    VULL_ASSERT(success);

    VULL_ASSERT(s_to_schedule == nullptr);
    s_to_schedule = dequeued;
    suspend();
}

} // namespace vull::tasklet
