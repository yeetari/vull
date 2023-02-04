#pragma once

#include <vull/support/Array.hh>
#include <vull/support/Optional.hh>
#include <vull/support/Utility.hh>

#include <stdint.h>

namespace vull {

class Semaphore;

template <typename Derived>
class TaskletBase {
    friend Derived;

private:
    void (*m_invoker)(const uint8_t *){nullptr};
    Optional<Semaphore &> m_semaphore;

    TaskletBase() = default;
    explicit TaskletBase(void (*invoker)(const uint8_t *)) : m_invoker(invoker) {}

public:
    void set_semaphore(Semaphore &semaphore) { m_semaphore = semaphore; }
};

// TODO: O(1) allocator for tasklets.
class Tasklet : public TaskletBase<Tasklet> {
    static constexpr auto k_inline_capacity = 64 - sizeof(TaskletBase);
    Array<uint8_t, k_inline_capacity> m_inline_storage;

    // Having a `requires` clause on both functions should not be strictly necessary, but it prevents accidental usage
    // of either of the functions if their prototypes differ for whatever reason, and also works around a (seeming)
    // bug in GCC that prevents compilation.
    template <typename F>
    static void invoke_helper(const uint8_t *inline_storage) requires(sizeof(F) <= k_inline_capacity) {
        // NOLINTNEXTLINE: const_cast
        const_cast<F &>((reinterpret_cast<const F &>(*inline_storage)))();
    }
    template <typename F>
    static void invoke_helper(const uint8_t *inline_storage) requires(sizeof(F) > k_inline_capacity) {
        // NOLINTNEXTLINE: const_cast
        const_cast<F &>((*reinterpret_cast<const F *const &>(*inline_storage)))();
    }

    // clang-format off
public:
    Tasklet() = default;
    template <typename F>
    // NOLINTNEXTLINE: this constructor does not shadow the move constructor
    Tasklet(F &&callable) requires(!is_same<F, Tasklet>);
    // clang-format on

    void invoke();
};

template <typename F>
// NOLINTNEXTLINE: this constructor does not shadow the move constructor
Tasklet::Tasklet(F &&callable) requires(!is_same<F, Tasklet>) : TaskletBase<Tasklet>(&invoke_helper<F>) {
    if constexpr (sizeof(F) <= k_inline_capacity) {
        new (m_inline_storage.data()) F(vull::forward<F>(callable));
    } else {
        new (m_inline_storage.data()) F *(new F(vull::forward<F>(callable)));
    }
}

void schedule(Tasklet &&tasklet, Optional<Semaphore &> semaphore = {});

} // namespace vull
