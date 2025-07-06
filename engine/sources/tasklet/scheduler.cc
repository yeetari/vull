#include <vull/tasklet/scheduler.hh>

#include <vull/container/array.hh>
#include <vull/container/mpmc_queue.hh>
#include <vull/container/vector.hh>
#include <vull/core/log.hh>
#include <vull/maths/common.hh>
#include <vull/platform/semaphore.hh>
#include <vull/platform/thread.hh>
#include <vull/support/assert.hh>
#include <vull/support/atomic.hh>
#include <vull/support/result.hh>
#include <vull/support/string_builder.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>
#include <vull/tasklet/functions.hh>
#include <vull/tasklet/tasklet.hh>

#include <stdint.h>
#include <unistd.h>

namespace vull::tasklet {
namespace {

extern "C" void vull_make_context(void *stack_top, void (*entry_point)(Tasklet *));
extern "C" [[noreturn]] void vull_load_context(Tasklet *tasklet, Tasklet *to_free);
extern "C" void vull_swap_context(Tasklet *from, Tasklet *to);

VULL_GLOBAL(thread_local Tasklet *s_current_tasklet = nullptr);
VULL_GLOBAL(thread_local Tasklet *s_scheduler_tasklet = nullptr);
VULL_GLOBAL(thread_local Tasklet *s_to_schedule = nullptr);
VULL_GLOBAL(thread_local TaskletQueue *s_queue = nullptr);
VULL_GLOBAL(thread_local Scheduler *s_scheduler = nullptr);
VULL_GLOBAL(thread_local platform::Semaphore *s_work_available = nullptr);

} // namespace

class TaskletQueue : public MpmcQueue<Tasklet *, 11> {};

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
    m_worker_threads.ensure_size(thread_count);
    m_queue = vull::make_unique<TaskletQueue>();
}

Scheduler::~Scheduler() {
    join();
}

void Scheduler::join() {
    m_running.store(false, vull::memory_order_release);
    while (m_alive_worker_count.load() != 0) {
        m_work_available.post();
    }
    m_worker_threads.clear();
}

uint32_t Scheduler::tasklet_count() const {
    return m_queue->size();
}

static Tasklet *pick_next() {
    auto *next = vull::exchange(s_to_schedule, nullptr);
    if (next == nullptr) {
        s_work_available->wait();
        while (true) {
            if (!s_scheduler->is_running() && s_queue->empty()) {
                s_scheduler->decrease_worker_count();
                platform::Thread::exit();
            }
            if ((next = s_queue->try_dequeue()) != nullptr) {
                break;
            }

            // The dequeue has failed, likely due to heavy contention. Yield to the OS scheduler to try to lift some of
            // it. This seems to improve performance in the event of very heavy mutex contention.
            platform::Thread::yield();
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

bool Scheduler::start(Tasklet *tasklet) {
    if (m_worker_threads.empty()) {
        return false;
    }
    if (!m_queue->try_enqueue(tasklet)) {
        return false;
    }
    m_running.store(true, vull::memory_order_release);
    for (uint32_t i = 0; i < m_worker_threads.size(); i++) {
        auto &thread = m_worker_threads[i];
        thread = VULL_EXPECT(platform::Thread::create([this] {
            // Setup per-thread data.
            s_scheduler = this;
            s_queue = m_queue.ptr();
            s_work_available = &m_work_available;
            VULL_EXPECT(platform::Thread::setup_signal_stack());

            // Use thread stack for scheduler tasklet.
            Array<uint8_t, 131072> tasklet_data{};
            s_scheduler_tasklet = new (tasklet_data.data()) Tasklet(tasklet_data.size(), nullptr);
            vull_make_context(s_scheduler_tasklet->stack_top(), scheduler_fn);

            m_alive_worker_count.fetch_add(1);
            auto *next = pick_next();
            vull_load_context(s_current_tasklet = next, nullptr);
        }));
        if (thread.pin_to_core(i).is_error()) {
            vull::error("[tasklet] Failed to pin worker thread {}", i);
        }
        VULL_IGNORE(thread.set_name(vull::format("Worker #{}", i)));
    }
    vull::info("[tasklet] Started {} threads", m_worker_threads.size());
    return true;
}

void Scheduler::setup_thread() {
    s_queue = m_queue.ptr();
    s_work_available = &m_work_available;
}

bool in_tasklet_context() {
    return s_current_tasklet != nullptr;
}

void schedule(Tasklet *tasklet) {
    VULL_ASSERT(s_queue != nullptr && s_work_available != nullptr);
    s_queue->enqueue(tasklet, yield);
    s_work_available->post();
}

void suspend() {
    VULL_ASSERT(s_current_tasklet->state() == TaskletState::Running);
    vull_swap_context(s_current_tasklet, s_scheduler_tasklet);
}

void yield() {
    auto *dequeued = s_queue->try_dequeue();
    if (dequeued == nullptr) {
        return;
    }
    [[maybe_unused]] bool success = s_queue->try_enqueue(s_current_tasklet);
    VULL_ASSERT(success);

    VULL_ASSERT(s_to_schedule == nullptr);
    s_to_schedule = dequeued;
    suspend();
}

} // namespace vull::tasklet
