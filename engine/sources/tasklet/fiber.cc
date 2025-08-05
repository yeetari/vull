#include <vull/tasklet/fiber.hh>

#include <vull/core/tracing.hh>
#include <vull/maths/common.hh>
#include <vull/platform/tasklet.hh>
#include <vull/support/atomic.hh>
#include <vull/support/span.hh>
#include <vull/support/string.hh>
#include <vull/support/utility.hh>

#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#if VULL_ASAN_ENABLED
#include <sanitizer/asan_interface.h>
#endif

namespace vull::tasklet {
namespace {

constexpr size_t k_max_stack_size = 1024uz * 1024;

VULL_GLOBAL(thread_local Fiber *s_current_fiber = nullptr);

extern "C" void vull_make_fiber(void *top, void (*entry_point)());
extern "C" void vull_switch_fiber(void *from, void *to);

} // namespace

Fiber *Fiber::create(void (*entry_point)(), String &&name) {
    auto *memory = platform::allocate_fiber_memory(k_max_stack_size);
    auto *fiber = new (memory - sizeof(Fiber)) Fiber(vull::move(name), memory - k_max_stack_size);
    vull_make_fiber(fiber, entry_point);
    return fiber;
}

Fiber *Fiber::current() {
    return s_current_fiber;
}

void Fiber::finish_switch([[maybe_unused]] Fiber *fiber) {
    tracing::enter_fiber(fiber->m_name.data());
#if VULL_ASAN_ENABLED
    __sanitizer_finish_switch_fiber(fiber->m_fake_stack_ptr, nullptr, nullptr);
#endif
}

uint32_t Fiber::advance_priority(Span<const uint32_t> priority_weights) {
    const auto current_level = m_priority_level;
    if (--m_priority_weight_counter == 0) {
        m_priority_level = (m_priority_level + 1) % priority_weights.size();
        m_priority_weight_counter = priority_weights[m_priority_level];
    }
    return current_level;
}

FiberState Fiber::exchange_state(FiberState state) {
    return m_state.exchange(state, vull::memory_order_relaxed);
}

[[noreturn]] void Fiber::switch_to() {
#if VULL_ASAN_ENABLED
    // We're not switching back so delete the fake stack.
    __sanitizer_start_switch_fiber(nullptr, m_memory_bottom, k_max_stack_size);
    m_fake_stack_ptr = nullptr;
#endif
    s_current_fiber = this;
    m_state.store(FiberState::Running, vull::memory_order_relaxed);
    vull_switch_fiber(nullptr, this);
    vull::unreachable();
}

void Fiber::swap_to(bool exchange_current) {
#if VULL_ASAN_ENABLED
    if (s_current_fiber != nullptr) {
        __sanitizer_start_switch_fiber(&s_current_fiber->m_fake_stack_ptr, m_memory_bottom, k_max_stack_size);
    } else {
        __sanitizer_start_switch_fiber(nullptr, m_memory_bottom, k_max_stack_size);
    }
#endif
    m_state.store(FiberState::Running, vull::memory_order_relaxed);
    if (exchange_current) {
        vull_switch_fiber(vull::exchange(s_current_fiber, this), this);
    } else {
        vull_switch_fiber(s_current_fiber, this);
    }
    Fiber::finish_switch(s_current_fiber);
}

bool Fiber::is_guard_page(uintptr_t address) const {
    const auto page_size = static_cast<uintptr_t>(getpagesize());
    return vull::align_down(address, page_size) == vull::bit_cast<uintptr_t>(m_memory_bottom);
}

bool Fiber::is_running() const {
    return m_state.load(vull::memory_order_relaxed) == FiberState::Running;
}

} // namespace vull::tasklet
