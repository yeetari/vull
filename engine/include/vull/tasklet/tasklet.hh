#pragma once

#include <vull/support/atomic.hh>
#include <vull/support/utility.hh>

#include <stddef.h>
#include <stdint.h>

namespace vull {

enum class TaskletSize {
    Normal,
    Large,
};

enum class TaskletState {
    Uninitialised,
    Running,
    Waiting,
    Done,
};

class Tasklet {
    void (*m_invoker)(const uint8_t *);
    void *m_stack_top;
#if VULL_ASAN_ENABLED
    size_t m_stack_size;
    void *m_fake_stack{nullptr};
#endif
    void *m_pool{nullptr};
    Tasklet *m_linked_tasklet{nullptr};
    Atomic<TaskletState> m_state{TaskletState::Uninitialised};

    template <typename F>
    static void invoke_helper(const uint8_t *inline_storage) {
        // NOLINTNEXTLINE: const_cast
        const_cast<F &>((reinterpret_cast<const F &>(*inline_storage)))();
    }

public:
    template <TaskletSize Size>
    static Tasklet *create();
    static Tasklet *current();
    bool is_guard_page(uintptr_t page) const;

    Tasklet(size_t size, void *pool);
    Tasklet(const Tasklet &) = delete;
    Tasklet(Tasklet &&) = delete;
    ~Tasklet() = delete;

    Tasklet &operator=(const Tasklet &) = delete;
    Tasklet &operator=(Tasklet &&) = delete;

    void set_linked_tasklet(Tasklet *tasklet) { m_linked_tasklet = tasklet; }
    Tasklet *pop_linked_tasklet() { return vull::exchange(m_linked_tasklet, nullptr); }

    // TODO: Remove this.
    Tasklet *linked_tasklet() const { return m_linked_tasklet; }

    void invoke();
    template <typename F>
    void set_callable(F &&callable);
    void set_state(TaskletState state) { m_state.store(state); }

    void *stack_top() const { return m_stack_top; }
    void *pool() const { return m_pool; }
    TaskletState state() const { return m_state.load(); }
};

inline Tasklet::Tasklet(size_t size, void *pool) : m_pool(pool) {
    m_stack_top = reinterpret_cast<uint8_t *>(this) + size;
}

inline void Tasklet::invoke() {
    m_invoker(reinterpret_cast<uint8_t *>(this + 1));
}

template <typename F>
void Tasklet::set_callable(F &&callable) {
    new (this + 1) F(vull::forward<F>(callable));
    m_invoker = &invoke_helper<F>;
#if VULL_ASAN_ENABLED
    const auto size =
        reinterpret_cast<ptrdiff_t>(reinterpret_cast<uint8_t *>(m_stack_top) - reinterpret_cast<uint8_t *>(this));
    m_stack_size = size - sizeof(Tasklet) - sizeof(F);
#endif
}

void pump_work();
bool try_schedule(Tasklet *tasklet);
void schedule(Tasklet *tasklet);
void yield();

template <typename F, TaskletSize Size = TaskletSize::Normal>
bool try_schedule(F &&callable) {
    auto *tasklet = Tasklet::create<Size>();
    if (tasklet == nullptr) {
        return false;
    }
    tasklet->set_callable(vull::forward<F>(callable));
    return try_schedule(tasklet);
}

template <typename F, TaskletSize Size = TaskletSize::Normal>
void schedule(F &&callable) {
    auto *tasklet = Tasklet::create<Size>();
    while (tasklet == nullptr) {
        pump_work();
        tasklet = Tasklet::create<Size>();
    }
    tasklet->set_callable(vull::forward<F>(callable));
    schedule(tasklet);
}

} // namespace vull
