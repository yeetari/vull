#include <vull/tasklet/Scheduler.hh>

#include <vull/support/Assert.hh>
#include <vull/support/Atomic.hh>
#include <vull/support/Optional.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>
#include <vull/support/WorkStealingQueue.hh>
#include <vull/tasklet/Tasklet.hh>

#include <pthread.h>
#include <time.h>
#include <unistd.h>

namespace vull {

class Semaphore;
class TaskletQueue : public WorkStealingQueue<Tasklet> {};
VULL_GLOBAL(static thread_local TaskletQueue *s_queue = nullptr);

void schedule(Tasklet &&tasklet, Optional<Semaphore &> semaphore) {
    VULL_ASSERT_PEDANTIC(s_queue != nullptr);
    if (semaphore) {
        tasklet.set_semaphore(*semaphore);
    }
    while (!s_queue->enqueue(move(tasklet))) {
        auto queued_tasklet = s_queue->dequeue();
        VULL_ASSERT(queued_tasklet);
        queued_tasklet->invoke();
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
        thread_count = static_cast<uint32_t>(sysconf(_SC_NPROCESSORS_ONLN)) / 2;
    }
    for (uint32_t i = 0; i < thread_count; i++) {
        auto &worker = m_workers.emplace(new Worker{.scheduler = *this});
        worker->queue = make_unique<TaskletQueue>();
        worker->rng_state = static_cast<uint32_t>(time(nullptr));
        worker->running.store(true, MemoryOrder::Relaxed);
    }
}

Scheduler::~Scheduler() {
    for (auto &worker : m_workers) {
        pthread_join(worker->thread, nullptr);
    }
}

bool Scheduler::start(Tasklet &&tasklet) {
    if (m_workers.empty()) {
        return false;
    }
    if (!m_workers[0]->queue->enqueue(move(tasklet))) {
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
    for (auto &worker : m_workers) {
        worker->running.store(false, MemoryOrder::Relaxed);
    }
}

void *Scheduler::thread_loop(void *init_data) {
    auto &[scheduler, queue, _, rng_state, running] = *static_cast<Worker *>(init_data);
    s_queue = queue.ptr();
    while (running.load(MemoryOrder::Relaxed) || !queue->empty()) {
        if (auto tasklet = queue->dequeue()) {
            tasklet->invoke();
            continue;
        }
        const auto &victim = scheduler.pick_victim(rng_state);
        if (&victim == init_data) {
            VULL_ASSERT_PEDANTIC(!victim.queue->steal());
            continue;
        }
        if (auto tasklet = victim.queue->steal()) {
            tasklet->invoke();
            continue;
        }
        usleep(100);
    }
    return nullptr;
}

} // namespace vull
