#include <vull/tasklet/Scheduler.hh>

#include <vull/core/Log.hh>
#include <vull/maths/Common.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Atomic.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>
#include <vull/support/WorkStealingQueue.hh>
#include <vull/tasklet/Tasklet.hh>

#include <pthread.h>
#include <semaphore.h>
#include <sys/random.h>
#include <unistd.h>

namespace vull {

extern "C" void vull_make_context(void *stack_top, void (*entry_point)(Tasklet *));
extern "C" [[noreturn]] void vull_load_context(Tasklet *tasklet, Tasklet *to_free);
extern "C" void vull_swap_context(Tasklet *from, Tasklet *to);

class TaskletQueue : public WorkStealingQueue<Tasklet *> {};
VULL_GLOBAL(static thread_local Tasklet *s_current_tasklet = nullptr);
VULL_GLOBAL(static thread_local Tasklet *s_scheduler_tasklet = nullptr);
VULL_GLOBAL(static thread_local Tasklet *s_to_schedule = nullptr);
VULL_GLOBAL(static thread_local TaskletQueue *s_queue = nullptr);
VULL_GLOBAL(static thread_local Scheduler *s_scheduler = nullptr);
VULL_GLOBAL(static thread_local uint32_t s_rng_state = 0);
VULL_GLOBAL(static Atomic<bool> s_running);
VULL_GLOBAL(static sem_t s_work_available);

Tasklet *Tasklet::current() {
    return s_current_tasklet;
}

TaskletQueue &Scheduler::pick_victim(uint32_t &rng_state) {
    rng_state ^= rng_state << 13u;
    rng_state ^= rng_state >> 17u;
    rng_state ^= rng_state << 5u;
    return *m_workers[rng_state % m_workers.size()]->queue;
}

Scheduler::Scheduler(uint32_t thread_count) {
    if (thread_count == 0) {
        thread_count = vull::max(static_cast<uint32_t>(sysconf(_SC_NPROCESSORS_ONLN)) / 2, 2u);
    }
    sem_init(&s_work_available, 0, 1);
    for (uint32_t i = 0; i < thread_count; i++) {
        auto &worker = m_workers.emplace(new Worker{.scheduler = *this});
        worker->queue = vull::make_unique<TaskletQueue>();
    }
    vull::info("[tasklet] Created {} threads", thread_count);
}

Scheduler::~Scheduler() {
    for (auto &worker : m_workers) {
        pthread_join(worker->thread, nullptr);
    }
    sem_destroy(&s_work_available);
}

bool Scheduler::start(Tasklet *tasklet) {
    if (m_workers.empty()) {
        return false;
    }
    if (!m_workers[0]->queue->enqueue(tasklet)) {
        return false;
    }
    s_running.store(true);
    for (auto &worker : m_workers) {
        if (pthread_create(&worker->thread, nullptr, &worker_entry, worker.ptr()) != 0) {
            return false;
        }
    }
    return true;
}

void Scheduler::stop() {
    s_running.store(false);
    for (uint32_t i = 0; i < m_workers.size(); i++) {
        sem_post(&s_work_available);
    }
}

[[noreturn]] static void invoke_trampoline(Tasklet *tasklet) {
    // TODO: Destruct lambda captures.
    tasklet->invoke();
    tasklet->set_state(TaskletState::Done);
    vull_load_context(s_scheduler_tasklet, nullptr);
}

[[noreturn]] static void scheduler_fn(Tasklet *) {
    bool current_done = s_current_tasklet->state() == TaskletState::Done;
    if (!current_done) {
        s_current_tasklet->set_state(TaskletState::Waiting);
    }

    Tasklet *next = vull::exchange(s_to_schedule, nullptr);
    while (next == nullptr) {
        sem_wait(&s_work_available);
        if (!s_running.load() && s_queue->empty()) {
            pthread_exit(nullptr);
        }

        next = s_queue->dequeue();
        if (next == nullptr) {
            auto &victim_queue = s_scheduler->pick_victim(s_rng_state);
            if (&victim_queue != s_queue) {
                next = victim_queue.steal();
            }
        }
        if (next == nullptr) {
            sem_post(&s_work_available);
        } else if (next->state() == TaskletState::Running) {
            vull::schedule(next);
            next = nullptr;
        }
    }

    VULL_ASSERT(next->state() != TaskletState::Done);
    if (next->state() == TaskletState::Uninitialised) {
        vull_make_context(next->stack_top(), invoke_trampoline);
    }
    next->set_state(TaskletState::Running);

    auto *current = vull::exchange(s_current_tasklet, next);
    vull_load_context(next, current_done ? current : nullptr);
}

void *Scheduler::worker_entry(void *worker_ptr) {
    auto &[scheduler, queue, _] = *static_cast<Worker *>(worker_ptr);
    s_queue = queue.ptr();
    s_scheduler = &scheduler;

    while (s_rng_state == 0) {
        VULL_ENSURE(getrandom(&s_rng_state, sizeof(uint32_t), 0) == sizeof(uint32_t));
    }

    s_scheduler_tasklet = Tasklet::create();
    s_current_tasklet = s_scheduler_tasklet;
    vull_make_context(s_scheduler_tasklet->stack_top(), scheduler_fn);
    vull_load_context(s_scheduler_tasklet, nullptr);
}

void schedule(Tasklet *tasklet) {
    VULL_ASSERT_PEDANTIC(s_queue != nullptr);
    sem_post(&s_work_available);
    while (!s_queue->enqueue(tasklet)) {
        auto *dequeued = s_queue->dequeue();
        VULL_ASSERT(dequeued != nullptr);
        [[maybe_unused]] bool success = s_queue->enqueue(s_current_tasklet);
        VULL_ASSERT(success);

        VULL_ASSERT(s_to_schedule == nullptr);
        s_to_schedule = dequeued;
        yield();
    }
}

void yield() {
    VULL_ASSERT(s_current_tasklet->state() == TaskletState::Running);
    vull_swap_context(s_current_tasklet, s_scheduler_tasklet);
}

} // namespace vull
