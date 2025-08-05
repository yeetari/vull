#pragma once

#include <vull/support/atomic.hh>
#include <vull/support/span.hh>
#include <vull/support/string.hh>
#include <vull/support/utility.hh>

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
    String m_name;
    void *m_memory_bottom;
    [[maybe_unused]] void *m_fake_stack_ptr{nullptr};
    Tasklet *m_current_tasklet{nullptr};
    Atomic<FiberState> m_state{FiberState::Runnable};
    uint32_t m_priority_level{0};
    uint32_t m_priority_weight_counter{1};

    Fiber(String &&name, void *memory_bottom) : m_name(vull::move(name)), m_memory_bottom(memory_bottom) {}

public:
    static Fiber *create(void (*entry_point)(), String &&name);
    static Fiber *current();
    static void finish_switch(Fiber *fiber);

    void set_current_tasklet(Tasklet *current_tasklet) { m_current_tasklet = current_tasklet; }

    uint32_t advance_priority(Span<const uint32_t> priority_weights);
    FiberState exchange_state(FiberState state);
    void swap_to(bool exchange_current);
    [[noreturn]] void switch_to();

    bool is_guard_page(uintptr_t address) const;
    bool is_running() const;
    Tasklet *current_tasklet() const { return m_current_tasklet; }
    FiberState state() const { return m_state.load(); }
};

} // namespace vull::tasklet
