#pragma once

#include <vull/support/function.hh>
#include <vull/support/utility.hh>
#include <vull/tasklet/promise.hh>

namespace vull::tasklet {

class Fiber;

class Tasklet {
    void (*const m_invoker)(Tasklet *);
    Fiber *m_owner{nullptr};
    Tasklet *m_linked_tasklet{nullptr};

public:
    static Tasklet *current();

    explicit Tasklet(void (*invoker)(Tasklet *)) : m_invoker(invoker) {}

    void invoke();

    void set_owner(Fiber *owner) { m_owner = owner; }
    Fiber *owner() const { return m_owner; }
    bool has_owner() const { return m_owner != nullptr; }

    void set_linked_tasklet(Tasklet *tasklet) { m_linked_tasklet = tasklet; }
    Tasklet *pop_linked_tasklet() { return vull::exchange(m_linked_tasklet, nullptr); }
};

template <typename F>
class PromisedTasklet final : public SharedPromise<typename FunctionTraits<F>::result_type>, public Tasklet {
    F m_callable;

    static void invoke_trampoline(Tasklet *);

public:
    using result_type = FunctionTraits<F>::result_type;

    explicit PromisedTasklet(F &&callable) : Tasklet(&invoke_trampoline), m_callable(vull::forward<F>(callable)) {
        this->add_ref();
    }
};

template <typename F>
PromisedTasklet(F) -> PromisedTasklet<F>;

inline void Tasklet::invoke() {
    m_invoker(this);
}

template <typename F>
void PromisedTasklet<F>::invoke_trampoline(Tasklet *base) {
    auto *tasklet = static_cast<PromisedTasklet<F> *>(base);
    if constexpr (vull::is_same<result_type, void>) {
        tasklet->m_callable();
        tasklet->fulfill();
    } else {
        tasklet->fulfill(tasklet->m_callable());
    }
    tasklet->sub_ref();
}

} // namespace vull::tasklet
