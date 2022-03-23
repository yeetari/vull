#pragma once

#include <vull/support/Array.hh>

namespace vull {

template <typename Derived>
class TaskletBase {
    friend Derived;

private:
    void (*m_invoker)(void *);

    TaskletBase() = default;
    explicit TaskletBase(void (*invoker)(void *)) : m_invoker(invoker) {}
};

// TODO: Destruction is not properly handled; need a move constructor but firstly need a proper allocator.
class alignas(64) Tasklet : public TaskletBase<Tasklet> {
    static constexpr auto k_inline_capacity = 64 - sizeof(TaskletBase);
    Array<uint8_t, k_inline_capacity> m_inline_storage;

    // Having a `requires` clause on both functions should not be strictly necessary, but it prevents accidental usage
    // of either of the functions if their prototypes differ for whatever reason, and also works around a (seemingly)
    // bug in GCC that prevents compilation.
    template <typename F>
    static void invoke_helper(void *inline_storage) requires(sizeof(F) <= k_inline_capacity) {
        (reinterpret_cast<const F &>(*static_cast<const char *>(inline_storage)))();
    }
    template <typename F>
    [[deprecated("functor storage demoted to a heap allocation, performance will be pessimised")]] static void
    invoke_helper(void *inline_storage) requires(sizeof(F) > k_inline_capacity) {
        (*reinterpret_cast<const F *const &>(*static_cast<const char *>(inline_storage)))();
    }

    // clang-format off
public:
    Tasklet() = default;
    template <typename F>
    // NOLINTNEXTLINE: this constructor does not shadow the move constructor
    Tasklet(F &&callable) requires(!IsSame<F, Tasklet>);
    // clang-format on

    void invoke() { m_invoker(m_inline_storage.data()); }
};

template <typename F>
// NOLINTNEXTLINE: no need to initialise m_inline_storage
Tasklet::Tasklet(F &&callable) requires(!IsSame<F, Tasklet>) : TaskletBase<Tasklet>(&invoke_helper<F>) {
    if constexpr (sizeof(F) <= k_inline_capacity) {
        new (m_inline_storage.data()) F(forward<F>(callable));
    } else {
        new (m_inline_storage.data()) F *(new F(forward<F>(callable)));
    }
}

void schedule(Tasklet &&tasklet);

} // namespace vull
