#include <vull/tasklet/tasklet.hh>

#include <vull/maths/common.hh>
#include <vull/support/assert.hh>
#include <vull/support/atomic.hh>
#include <vull/support/utility.hh>
#include <vull/tasklet/functions.hh>

#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

// TODO(tasklet-perf): Benchmark per-thread memory pools vs a global pool (old global pool in git history).
// TODO: Stack guard pages wasting a lot of memory currently. Tasklet object (~40 bytes) rounded up to 4 KiB, then
//       another 4 KiB used for the guard page. Maybe tasklet object could go after the stack?
// TODO: Investigate on-demand growable stacks? (with MAP_GROWSDOWN | MAP_STACK)

namespace vull::tasklet {
namespace {

class PoolBase {
protected:
    const size_t m_stack_size{0};
    uint8_t *m_base{nullptr};
    size_t m_head{0};
    Atomic<void *> m_next_free{nullptr};

    explicit PoolBase(size_t stack_size) : m_stack_size(stack_size) {}

public:
    void free(Tasklet *tasklet);
    bool is_guard_page(uintptr_t page) const;
};

template <unsigned TaskletCount, size_t StackSize>
class Pool : public PoolBase {
public:
    Pool();

    Tasklet *allocate();
};

template <unsigned TaskletCount, size_t StackSize>
Pool<TaskletCount, StackSize>::Pool() : PoolBase(StackSize) {
    constexpr auto mmap_prot = PROT_READ | PROT_WRITE;
    constexpr auto mmap_flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE;
    const size_t total_size = StackSize * TaskletCount;
    void *mmap_result = mmap(nullptr, total_size, mmap_prot, mmap_flags, -1, 0);
    VULL_ENSURE(mmap_result != MAP_FAILED);
    m_base = static_cast<uint8_t *>(mmap_result);

    // Mark guard pages.
    const auto page_size = static_cast<size_t>(getpagesize());
    for (uint8_t *page = m_base; page < m_base + total_size; page += StackSize) {
        mprotect(page + page_size, page_size, PROT_NONE);
    }
}

template <unsigned TaskletCount, size_t StackSize>
Tasklet *Pool<TaskletCount, StackSize>::allocate() {
    auto *next_free = m_next_free.load();
    if (next_free != nullptr) {
        // Since we are the only thread allocating, m_next_free can never become null after this check.
        void *desired;
        do {
            VULL_ASSERT(next_free != nullptr);
            memcpy(&desired, next_free, sizeof(void *));
        } while (!m_next_free.compare_exchange_weak(next_free, desired));
        return new (next_free) Tasklet(StackSize, this);
    }

    if (m_head >= StackSize * TaskletCount) {
        // Pool full.
        return nullptr;
    }

    size_t head = vull::exchange(m_head, m_head + StackSize);
    return new (m_base + head) Tasklet(StackSize, this);
}

void PoolBase::free(Tasklet *tasklet) {
    void *next_free = m_next_free.load();
    do {
        memcpy(tasklet, &next_free, sizeof(void *));
    } while (!m_next_free.compare_exchange_weak(next_free, tasklet));
}

bool PoolBase::is_guard_page(uintptr_t page) const {
    const auto page_size = static_cast<uintptr_t>(getpagesize());
    page = vull::align_down(page, page_size);
    const auto base = reinterpret_cast<uintptr_t>(m_base);
    if (page < base + page_size) {
        return false;
    }
    return (page - base - page_size) % m_stack_size == 0;
}

VULL_GLOBAL(thread_local Pool<64, 65536> s_normal_pool);
VULL_GLOBAL(thread_local Pool<4, 262144> s_large_pool);

// These noinline forwarders exist so that the compiler is forced to reload the thread local data.
// TODO: This is quite hacky and could break in the future.

[[gnu::noinline]] Tasklet *allocate_normal() {
    return s_normal_pool.allocate();
}

[[gnu::noinline]] Tasklet *allocate_large() {
    return s_large_pool.allocate();
}

} // namespace

extern "C" void vull_free_tasklet(Tasklet *);
extern "C" void vull_free_tasklet(Tasklet *tasklet) {
    if (tasklet != nullptr) {
        VULL_ASSERT(tasklet->state() == TaskletState::Done);
        static_cast<PoolBase *>(tasklet->pool())->free(tasklet);
    }
}

template <>
Tasklet *Tasklet::create<TaskletSize::Normal>() {
    auto *tasklet = allocate_normal();
    while (tasklet == nullptr) {
        tasklet::yield();
        tasklet = allocate_normal();
    }
    return tasklet;
}

template <>
Tasklet *Tasklet::create<TaskletSize::Large>() {
    auto *tasklet = allocate_large();
    while (tasklet == nullptr) {
        tasklet::yield();
        tasklet = allocate_large();
    }
    return tasklet;
}

bool Tasklet::is_guard_page(uintptr_t page) const {
    return static_cast<PoolBase *>(m_pool)->is_guard_page(page);
}

} // namespace vull::tasklet
