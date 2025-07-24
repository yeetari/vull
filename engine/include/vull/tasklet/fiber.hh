#pragma once

#include <vull/support/atomic.hh>

#include <stdint.h>

namespace vull::tasklet {

class Tasklet;

enum class FiberState {
    Runnable,
    Running,
    Yielding,
    Suspended,
};

class Fiber {
    void *m_memory_bottom;
    Tasklet *m_current_tasklet{nullptr};
    Atomic<FiberState> m_state{FiberState::Runnable};

    explicit Fiber(void *memory_bottom) : m_memory_bottom(memory_bottom) {}

public:
    static Fiber *create(void (*entry_point)());
    static Fiber *current();

    void set_current_tasklet(Tasklet *current_tasklet) { m_current_tasklet = current_tasklet; }

    FiberState exchange_state(FiberState state);
    void swap_to(bool exchange_current);
    [[noreturn]] void switch_to();

    bool is_guard_page(uintptr_t address) const;
    bool is_running() const;
    Tasklet *current_tasklet() const { return m_current_tasklet; }
    FiberState state() const { return m_state.load(); }
};

} // namespace vull::tasklet
