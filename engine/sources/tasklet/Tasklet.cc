#include <vull/tasklet/Tasklet.hh>

#include <vull/maths/Common.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Atomic.hh>
#include <vull/support/Utility.hh>

#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

namespace vull {
namespace {

constexpr size_t k_pool_size = 256 * 1024 * 1024;
constexpr size_t k_size = 262144; // TODO: Decrease.

class Pool {
    uint8_t *m_base;
    Atomic<size_t> m_head;
    Atomic<void *> m_next_free{nullptr};

public:
    Pool();

    Tasklet *allocate();
    void free(Tasklet *tasklet);
    bool is_guard_page(uintptr_t page);
};

Pool::Pool() {
    m_base =
        static_cast<uint8_t *>(mmap(nullptr, k_pool_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));

    const auto page_size = static_cast<size_t>(getpagesize());
    for (uint8_t *page = m_base; page < m_base + k_pool_size; page += k_size) {
        mprotect(page + page_size, page_size, PROT_NONE);
    }
}

Tasklet *Pool::allocate() {
    auto *next_free = m_next_free.load();
    while (next_free != nullptr &&
           !m_next_free.compare_exchange_weak(next_free, *reinterpret_cast<void **>(next_free))) {
    }
    if (next_free != nullptr) {
        return new (next_free) Tasklet(k_size);
    }
    size_t head = m_head.fetch_add(k_size);
    VULL_ASSERT(head + k_size < k_pool_size);
    return new (m_base + head) Tasklet(k_size);
}

void Pool::free(Tasklet *tasklet) {
    void *next_free = m_next_free.load();
    do {
        memcpy(tasklet, &next_free, sizeof(void *));
    } while (!m_next_free.compare_exchange_weak(next_free, tasklet));
}

bool Pool::is_guard_page(uintptr_t page) {
    const auto page_size = static_cast<uintptr_t>(getpagesize());
    page = vull::align_down(page, page_size);
    const auto base = reinterpret_cast<uintptr_t>(m_base);
    if (page < base + page_size) {
        return false;
    }
    return (page - base - page_size) % k_size == 0;
}

VULL_GLOBAL(Pool s_pool);

} // namespace

extern "C" void vull_free_tasklet(Tasklet *);
extern "C" void vull_free_tasklet(Tasklet *tasklet) {
    if (tasklet != nullptr) {
        s_pool.free(tasklet);
    }
}

Tasklet *Tasklet::create() {
    return s_pool.allocate();
}

bool Tasklet::is_guard_page(uintptr_t page) {
    return s_pool.is_guard_page(page);
}

} // namespace vull
