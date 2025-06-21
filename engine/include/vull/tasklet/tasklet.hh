#pragma once

#include <vull/support/atomic.hh>
#include <vull/support/function.hh>
#include <vull/support/shared_ptr.hh>
#include <vull/support/utility.hh>
#include <vull/tasklet/promise.hh>

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

template <typename F>
class PromisedCallable {
    using R = FunctionTraits<F>::result_type;

private:
    // TODO: Instead of always heap allocating a promise, maybe a promise could always be stored in the tasklet and the
    // tasklet could be kept alive until the promise reference count is zero.
    F m_callable;
    SharedPtr<SharedPromise<R>> m_promise;

public:
    PromisedCallable(F &&callable) : m_callable(vull::forward<F>(callable)), m_promise(new SharedPromise<R>) {}
    PromisedCallable(const PromisedCallable &) = delete;
    PromisedCallable(PromisedCallable &&) = delete;
    ~PromisedCallable() = default;

    PromisedCallable &operator=(const PromisedCallable &) = delete;
    PromisedCallable &operator=(PromisedCallable &&) = delete;

    void invoke();
    SharedPtr<SharedPromise<R>> promise() const { return m_promise; }
};

template <typename F>
PromisedCallable(F) -> PromisedCallable<F>;

class Tasklet {
    void (*m_invoker)(Tasklet *);
    void *m_stack_top;
#if VULL_ASAN_ENABLED
    size_t m_stack_size;
    void *m_fake_stack{nullptr};
#endif
    void *m_pool{nullptr};
    Tasklet *m_linked_tasklet{nullptr};
    Atomic<TaskletState> m_state{TaskletState::Uninitialised};

    template <typename F>
    static void invoke_trampoline(Tasklet *);

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

    template <typename F>
    auto set_callable(F &&callable);
    void set_state(TaskletState state) { m_state.store(state); }

    void (*invoker())(Tasklet *) { return m_invoker; }
    void *stack_top() const { return m_stack_top; }
    void *pool() const { return m_pool; }
    TaskletState state() const { return m_state.load(); }
};

[[noreturn]] void tasklet_switch_next(Tasklet *current);

template <typename F>
void PromisedCallable<F>::invoke() {
    if constexpr (vull::is_same<R, void>) {
        m_callable();
        m_promise->fulfill();
    } else {
        m_promise->fulfill(m_callable());
    }
}

template <typename F>
void Tasklet::invoke_trampoline(Tasklet *tasklet) {
    if constexpr (!vull::is_same<F, void>) {
        auto &callable = *vull::bit_cast<PromisedCallable<F> *>(tasklet + 1);
        callable.invoke();
        callable.~PromisedCallable<F>();
    }

    // Mark tasklet as done and switch to the next tasklet context straight away.
    tasklet->set_state(TaskletState::Done);
    tasklet_switch_next(tasklet);
}

inline Tasklet::Tasklet(size_t size, void *pool) : m_pool(pool) {
    // Default to a no-op invoker.
    m_invoker = &invoke_trampoline<void>;
    m_stack_top = vull::bit_cast<uint8_t *>(this) + size;
}

template <typename F>
auto Tasklet::set_callable(F &&callable) {
    auto *promised = ::new (this + 1) PromisedCallable(vull::forward<F>(callable));
    m_invoker = &invoke_trampoline<F>;
#if VULL_ASAN_ENABLED
    const auto size = vull::bit_cast<size_t>(static_cast<uint8_t *>(m_stack_top) - vull::bit_cast<uint8_t *>(this));
    m_stack_size = size - sizeof(Tasklet) - sizeof(PromisedCallable<F>);
#endif
    return promised->promise();
}

} // namespace vull
