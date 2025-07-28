#include <vull/tasklet/scheduler.hh>

#include <vull/container/array.hh>
#include <vull/container/mpmc_queue.hh>
#include <vull/container/vector.hh>
#include <vull/core/log.hh>
#include <vull/maths/common.hh>
#include <vull/platform/event.hh>
#include <vull/platform/semaphore.hh>
#include <vull/platform/tasklet.hh>
#include <vull/platform/thread.hh>
#include <vull/support/assert.hh>
#include <vull/support/atomic.hh>
#include <vull/support/result.hh>
#include <vull/support/shared_ptr.hh>
#include <vull/support/string_builder.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>
#include <vull/tasklet/fiber.hh>
#include <vull/tasklet/functions.hh>
#include <vull/tasklet/io.hh>
#include <vull/tasklet/tasklet.hh>

#include <stdint.h>

namespace vull::tasklet {

class FiberQueue : public MpmcQueue<Fiber *, 9> {};
class TaskletQueue : public MpmcQueue<Tasklet *, 11> {};

namespace {

constexpr Array k_priority_weights{
    4u,
    1u,
};

// Shared between all worker threads of the same scheduler.
VULL_GLOBAL(thread_local FiberQueue *s_fiber_queue = nullptr);
VULL_GLOBAL(thread_local TaskletQueue *s_tasklet_queue = nullptr);
VULL_GLOBAL(thread_local Scheduler *s_scheduler = nullptr);
VULL_GLOBAL(thread_local platform::Semaphore *s_work_available = nullptr);
VULL_GLOBAL(thread_local Atomic<uint32_t> *s_ready_fiber_count = nullptr);
VULL_GLOBAL(thread_local Atomic<uint32_t> *s_ready_tasklet_count = nullptr);

// Per worker thread.
VULL_GLOBAL(thread_local Fiber *s_helper_fiber = nullptr);
VULL_GLOBAL(thread_local Fiber *s_cleanup_fiber = nullptr);

Fiber *pick_ready_fiber() {
    uint32_t count = s_ready_fiber_count->load(vull::memory_order_relaxed);
    while (true) {
        if (count == 0) {
            return nullptr;
        }
        if (s_ready_fiber_count->compare_exchange_weak(count, count - 1, vull::memory_order_acquire)) {
            // Yield to the OS scheduler in the event of a dequeue failure, which is likely due to heavy contention.
            // This seems to improve performance in the event of very heavy mutex contention.
            return s_fiber_queue->dequeue(platform::Thread::yield);
        }
    }
}

Tasklet *pick_ready_tasklet() {
    uint32_t count = s_ready_tasklet_count->load(vull::memory_order_relaxed);
    while (true) {
        if (count == 0) {
            return nullptr;
        }
        if (s_ready_tasklet_count->compare_exchange_weak(count, count - 1, vull::memory_order_acquire)) {
            return s_tasklet_queue->dequeue(platform::Thread::yield);
        }
    }
}

[[noreturn]] void fiber_loop() {
    auto *running_fiber = Fiber::current();
    Fiber::finish_switch(running_fiber);
    while (s_scheduler->is_running()) {
        // Wait for new work.
        s_work_available->wait();

        // Prioritise getting either a fiber or tasklet first but try to make sure we always get something since we've
        // decremented the semaphore.
        Fiber *fiber = nullptr;
        Tasklet *tasklet = nullptr;
        if (Fiber::current()->advance_priority(k_priority_weights.span()) == 0) {
            fiber = pick_ready_fiber();
            if (fiber == nullptr) {
                tasklet = pick_ready_tasklet();
            }
        } else {
            tasklet = pick_ready_tasklet();
            if (tasklet == nullptr) {
                fiber = pick_ready_fiber();
            }
        }

        if (fiber != nullptr) {
            VULL_ASSERT(fiber != running_fiber);

            // Mark the current fiber to be returned to the free pool.
            VULL_ASSERT(s_cleanup_fiber == nullptr);
            s_cleanup_fiber = running_fiber;

            // Switch to the unblocked fiber.
            fiber->swap_to(true);
        } else if (tasklet != nullptr) {
            // Suspended tasklets should resume via the fiber queue.
            VULL_ASSERT(!tasklet->has_owner());

            // Run the tasklet.
            running_fiber->set_current_tasklet(tasklet);
            tasklet->set_owner(running_fiber);
            tasklet->invoke();
            running_fiber->set_current_tasklet(nullptr);
        } else {
            // If we got here, than the semaphore had a higher count than the number of jobs available, probably because
            // of the ready fiber getting in the helper fiber.
            // TODO: Using a counting semaphore like this is suboptimal.
        }
    }

    s_scheduler->decrease_worker_count();
    platform::Thread::exit();
}

void unsuspend_fiber(Fiber *fiber) {
    // The fiber may still be in the middle of yielding, spin whilst that's the case.
    // TODO: Can we do something better here?
    FiberState state;
    do {
        state = fiber->state();
    } while (state == FiberState::Running || state == FiberState::Yielding);

    [[maybe_unused]] const auto old_state = fiber->exchange_state(FiberState::Runnable);
    VULL_ASSERT(old_state == FiberState::Suspended);
    s_fiber_queue->enqueue(fiber, [] {});
    s_ready_fiber_count->fetch_add(1, vull::memory_order_release);
    s_work_available->post();
}

[[noreturn]] void helper_fiber() {
    // Finalise the stack switch for ASan.
    Fiber::finish_switch(s_helper_fiber);

    // Get the fiber we switched from.
    auto *fiber = Fiber::current();
    if (fiber != nullptr && fiber->exchange_state(FiberState::Suspended) == FiberState::Yielding) {
        // Requeue straight away. This shouldn't ever block.
        unsuspend_fiber(fiber);
    }

    // Pick a new fiber to switch to. This may end up picking the fiber we just yielded from, but that's better than
    // making a new fiber. Also the FIFO guarantee of the queue means that it has lowest priority.
    Fiber *next_fiber;
    while (true) {
        // If priority allows, try to pick an unblocked fiber first.
        const bool fiber_first = fiber != nullptr && fiber->advance_priority(k_priority_weights.span()) == 0;
        if (fiber_first && (next_fiber = pick_ready_fiber()) != nullptr) {
            break;
        }

        // Otherwise try to get a free fiber from the pool.
        if ((next_fiber = s_scheduler->request_fiber()) != nullptr) {
            break;
        }

        // The pool is full so we must try to pick any fiber now.
        if ((next_fiber = pick_ready_fiber()) != nullptr) {
            break;
        }

        // Otherwise we've exhausted the maximum fiber count and there's not much we can do.
        // TODO: Rate limit this log message.
        vull::warn("[tasklet] Exhausted the fiber pool");
        platform::Thread::yield();
    }

    // Switch to the fiber.
    next_fiber->switch_to();
}

} // namespace

Scheduler &Scheduler::current() {
    return *s_scheduler;
}

Scheduler::Scheduler(uint32_t thread_count, uint32_t fiber_limit, bool pin_threads) {
    m_fiber_limit = vull::clamp(fiber_limit, thread_count + 1, FiberQueue::capacity());
    if (fiber_limit != m_fiber_limit) {
        vull::warn("[tasklet] Fiber limit clamped to {}", m_fiber_limit);
    } else {
        vull::debug("[tasklet] Fiber limit set to {}", m_fiber_limit);
    }

    m_ready_fiber_queue = vull::make_unique<FiberQueue>();
    m_free_fiber_queue = vull::make_unique<FiberQueue>();
    m_tasklet_queue = vull::make_unique<TaskletQueue>();
    m_io_queue = vull::make_unique<IoQueue>();

    for (uint32_t i = 0; i < thread_count; i++) {
        auto thread = VULL_EXPECT(platform::Thread::create([this] {
            // Setup per-thread data.
            setup_thread();
            VULL_EXPECT(platform::Thread::setup_signal_stack());

            // Create a helper fiber per thread.
            m_alive_worker_count.fetch_add(1);
            s_helper_fiber = Fiber::create(&helper_fiber);
            s_helper_fiber->swap_to(false);
        }));
        VULL_IGNORE(thread.set_name(vull::format("Worker #{}", i)));
        if (pin_threads && thread.pin_to_core(i).is_error()) {
            vull::error("[tasklet] Failed to pin worker thread {}", i);
        }
        m_worker_threads.push(vull::move(thread));
    }
    vull::info("[tasklet] Created {} worker threads", m_worker_threads.size());

    m_io_thread = VULL_EXPECT(platform::Thread::create([this] {
        setup_thread();
        platform::spawn_tasklet_io_dispatcher(*m_io_queue);
    }));
    VULL_IGNORE(m_io_thread.set_name("IO"));
    vull::info("[tasklet] Created IO thread");
}

Scheduler::~Scheduler() {
    join();
}

void Scheduler::join() {
    // Join worker threads.
    m_running.store(false, vull::memory_order_release);
    while (m_alive_worker_count.load() != 0) {
        m_work_available.post();
    }
    m_worker_threads.clear();

    // Join the IO thread last in case there are tasklets waiting for IO.
    m_io_queue->quit_event.set();
    VULL_EXPECT(m_io_thread.join());
}

Fiber *Scheduler::request_fiber() {
    // Try to get a free fiber.
    if (auto *fiber = m_free_fiber_queue->try_dequeue()) {
        return fiber;
    }

    // Otherwise create a new fiber if we're under the limit.
    uint32_t fiber_count = m_created_fiber_count.load(vull::memory_order_relaxed);
    do {
        if (fiber_count >= m_fiber_limit) {
            return nullptr;
        }
    } while (!m_created_fiber_count.compare_exchange_weak(fiber_count, fiber_count + 1, vull::memory_order_acq_rel,
                                                          vull::memory_order_acquire));
    if ((fiber_count + 1) % 10 == 0) {
        vull::trace("[tasklet] Reached {} allocated fibers", fiber_count + 1);
    }
    return Fiber::create(&fiber_loop);
}

void Scheduler::return_fiber(Fiber *fiber) {
    VULL_ASSERT(fiber->state() == FiberState::Runnable);
    m_free_fiber_queue->enqueue(fiber, [] {});
}

void Scheduler::setup_thread() {
    s_scheduler = this;
    s_fiber_queue = m_ready_fiber_queue.ptr();
    s_tasklet_queue = m_tasklet_queue.ptr();
    s_work_available = &m_work_available;
    s_ready_fiber_count = &m_ready_fiber_count;
    s_ready_tasklet_count = &m_ready_tasklet_count;
}

void Scheduler::enqueue(Tasklet *tasklet) {
    m_tasklet_queue->enqueue(tasklet, tasklet::yield);
    m_ready_tasklet_count.fetch_add(1, vull::memory_order_release);
    m_work_available.post();
}

void Scheduler::submit_io_request(SharedPtr<IoRequest> request) {
    m_io_queue->enqueue(request.disown(), tasklet::yield);
    if (m_io_queue->pending.fetch_add(1, vull::memory_order_relaxed) == 0) {
        m_io_queue->submit_event.set();
    }
}

uint32_t Scheduler::queued_tasklet_count() const {
    return m_tasklet_queue->size();
}

bool Scheduler::is_running() const {
    return m_running.load(vull::memory_order_acquire);
}

bool in_tasklet_context() {
    return Fiber::current() != nullptr;
}

void schedule(Tasklet *tasklet) {
    if (!tasklet->has_owner()) {
        // New tasklet goes on the queue.
        s_scheduler->enqueue(tasklet);
    } else {
        // Otherwise unsuspend its owner fiber.
        VULL_ASSERT(s_fiber_queue != nullptr);
        unsuspend_fiber(tasklet->owner());
    }
}

void submit_io_request(SharedPtr<IoRequest> request) {
    VULL_ASSERT(in_tasklet_context());
    s_scheduler->submit_io_request(vull::move(request));
}

void suspend() {
    // Switch to the helper fiber.
    VULL_ASSERT(in_tasklet_context());
    VULL_ASSERT(Fiber::current() != s_helper_fiber);
    s_helper_fiber->swap_to(false);

    // We've been unsuspended, check if we need to return the previously running fiber to the free pool.
    if (auto *fiber = vull::exchange(s_cleanup_fiber, nullptr)) {
        fiber->exchange_state(FiberState::Runnable);
        s_scheduler->return_fiber(fiber);
    }
}

void yield() {
    if (!in_tasklet_context()) [[unlikely]] {
        // Just yield to the OS scheduler.
        platform::Thread::yield();
        return;
    }
    [[maybe_unused]] const auto old_state = Fiber::current()->exchange_state(FiberState::Yielding);
    VULL_ASSERT(old_state == FiberState::Running);
    suspend();
}

} // namespace vull::tasklet
