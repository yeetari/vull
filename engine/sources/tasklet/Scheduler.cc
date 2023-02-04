#include <vull/tasklet/Scheduler.hh>

#include <vull/core/Log.hh>
#include <vull/maths/Common.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Atomic.hh>
#include <vull/support/Optional.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>
#include <vull/support/WorkStealingQueue.hh>
#include <vull/tasklet/Tasklet.hh>

#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

namespace vull {

class Semaphore;
class TaskletQueue : public WorkStealingQueue<Tasklet *> {};
VULL_GLOBAL(static thread_local TaskletQueue *s_queue = nullptr);
VULL_GLOBAL(static sem_t s_work_available);

void schedule(Tasklet &&tasklet, Optional<Semaphore &> semaphore) {
    VULL_ASSERT_PEDANTIC(s_queue != nullptr);
    if (semaphore) {
        tasklet.set_semaphore(*semaphore);
    }
    sem_post(&s_work_available);
    while (!s_queue->enqueue(new Tasklet(vull::move(tasklet)))) {
        auto *queued_tasklet = s_queue->dequeue();
        VULL_ASSERT(queued_tasklet != nullptr);
        queued_tasklet->invoke();
        delete queued_tasklet;
    }
}

Scheduler::Worker &Scheduler::pick_victim(uint32_t &rng_state) {
    rng_state ^= rng_state << 13u;
    rng_state ^= rng_state >> 17u;
    rng_state ^= rng_state << 5u;
    return *m_workers[rng_state % m_workers.size()];
}

Scheduler::Scheduler(uint32_t thread_count) {
    if (thread_count == 0) {
        thread_count = vull::max(static_cast<uint32_t>(sysconf(_SC_NPROCESSORS_ONLN)) / 2, 2u);
    }
    sem_init(&s_work_available, 0, 1);
    for (uint32_t i = 0; i < thread_count; i++) {
        auto &worker = m_workers.emplace(new Worker{.scheduler = *this});
        worker->queue = vull::make_unique<TaskletQueue>();
        worker->rng_state = i + 1;
        worker->running.store(true, MemoryOrder::Relaxed);
    }
    vull::info("[tasklet] Created {} threads", thread_count);
}

Scheduler::~Scheduler() {
    for (auto &worker : m_workers) {
        pthread_join(worker->thread, nullptr);
    }
    sem_destroy(&s_work_available);
}

bool Scheduler::start(Tasklet &&tasklet) {
    if (m_workers.empty()) {
        return false;
    }
    if (!m_workers[0]->queue->enqueue(new Tasklet(vull::move(tasklet)))) {
        return false;
    }
    for (auto &worker : m_workers) {
        if (pthread_create(&worker->thread, nullptr, &thread_loop, worker.ptr()) != 0) {
            return false;
        }
    }
    return true;
}

void Scheduler::stop() {
    sem_post(&s_work_available);
    for (auto &worker : m_workers) {
        worker->running.store(false, MemoryOrder::Relaxed);
    }
}

void *Scheduler::thread_loop(void *init_data) {
    auto &[scheduler, queue, _, rng_state, running] = *static_cast<Worker *>(init_data);
    s_queue = queue.ptr();
    while (running.load(MemoryOrder::Relaxed) || !queue->empty()) {
        sem_wait(&s_work_available);
        if (auto *tasklet = queue->dequeue()) {
            tasklet->invoke();
            delete tasklet;
            continue;
        }
        const auto &victim = scheduler.pick_victim(rng_state);
        if (&victim == init_data) {
            sem_post(&s_work_available);
            continue;
        }
        if (auto *tasklet = victim.queue->steal()) {
            tasklet->invoke();
            delete tasklet;
            continue;
        }
        sem_post(&s_work_available);
    }
    return nullptr;
}

} // namespace vull
