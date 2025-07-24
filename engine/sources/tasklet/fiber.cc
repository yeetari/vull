#include <vull/tasklet/fiber.hh>

#include <vull/maths/common.hh>
#include <vull/platform/tasklet.hh>
#include <vull/support/atomic.hh>
#include <vull/support/utility.hh>

#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

namespace vull::tasklet {
namespace {

constexpr size_t k_max_stack_size = 1024uz * 1024;

VULL_GLOBAL(thread_local Fiber *s_current_fiber = nullptr);

extern "C" void vull_make_fiber(void *top, void (*entry_point)());
extern "C" void vull_switch_fiber(void *from, void *to);

} // namespace

Fiber *Fiber::create(void (*entry_point)()) {
    auto *memory = platform::allocate_fiber_memory(k_max_stack_size);
    auto *fiber = new (memory - sizeof(Fiber)) Fiber(memory - k_max_stack_size);
    vull_make_fiber(fiber, entry_point);
    return fiber;
}

Fiber *Fiber::current() {
    return s_current_fiber;
}

FiberState Fiber::exchange_state(FiberState state) {
    return m_state.exchange(state, vull::memory_order_relaxed);
}

[[noreturn]] void Fiber::switch_to() {
    s_current_fiber = this;
    m_state.store(FiberState::Running, vull::memory_order_relaxed);
    vull_switch_fiber(nullptr, this);
    vull::unreachable();
}

void Fiber::swap_to(bool exchange_current) {
    m_state.store(FiberState::Running, vull::memory_order_relaxed);
    if (exchange_current) {
        vull_switch_fiber(vull::exchange(s_current_fiber, this), this);
    } else {
        vull_switch_fiber(s_current_fiber, this);
    }
}

bool Fiber::is_guard_page(uintptr_t address) const {
    const auto page_size = static_cast<uintptr_t>(getpagesize());
    return vull::align_down(address, page_size) == vull::bit_cast<uintptr_t>(m_memory_bottom);
}

bool Fiber::is_running() const {
    return m_state.load(vull::memory_order_relaxed) == FiberState::Running;
}

} // namespace vull::tasklet
