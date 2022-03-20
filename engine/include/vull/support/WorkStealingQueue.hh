#pragma once

#include <vull/support/Array.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Atomic.hh>
#include <vull/support/Optional.hh>
#include <vull/support/Utility.hh>

#include <stdint.h>

namespace vull {

template <typename T, unsigned SlotCountShift = 10>
class WorkStealingQueue {
    // Max size of the queue, should be a power of two to allow modulo operations to be transformed into cheaper ands.
    static constexpr int64_t k_slot_count = 1ul << SlotCountShift;
    Array<T, k_slot_count> m_slots{};
    Atomic<int64_t> m_head;
    Atomic<int64_t> m_tail;

public:
    [[nodiscard]] bool enqueue(T &&elem);
    Optional<T> dequeue();
    Optional<T> steal();

    bool empty() const;
    uint64_t size() const;
};

template <typename T, unsigned SlotCountShift>
[[nodiscard]] bool WorkStealingQueue<T, SlotCountShift>::enqueue(T &&elem) {
    int64_t head = m_head.load(MemoryOrder::Relaxed);
    int64_t tail = m_tail.load(MemoryOrder::Acquire);
    if (head - tail >= k_slot_count) {
        return false;
    }

    atomic_store(m_slots[static_cast<uint32_t>(head % k_slot_count)], forward<T>(elem));
    m_head.store(head + 1, MemoryOrder::Relaxed);
    return true;
}

template <typename T, unsigned SlotCountShift>
Optional<T> WorkStealingQueue<T, SlotCountShift>::dequeue() {
    int64_t index = m_head.fetch_sub(1, MemoryOrder::Relaxed) - 1;
    int64_t tail = m_tail.load(MemoryOrder::Relaxed);
    if (tail <= index) {
        if (tail == index) {
            m_head.store(index + 1, MemoryOrder::Relaxed);
            if (!m_tail.compare_exchange(tail, tail + 1, MemoryOrder::SeqCst, MemoryOrder::Relaxed)) {
                // Element was just stolen.
                return {};
            }
        }
        return atomic_load(m_slots[static_cast<uint32_t>(index % k_slot_count)]);
    }
    m_head.store(index + 1, MemoryOrder::Relaxed);
    return {};
}

template <typename T, unsigned SlotCountShift>
Optional<T> WorkStealingQueue<T, SlotCountShift>::steal() {
    int64_t tail = m_tail.load(MemoryOrder::Acquire);
    int64_t head = m_head.load(MemoryOrder::Acquire);

    if (tail < head) {
        if (!m_tail.compare_exchange(tail, tail + 1, MemoryOrder::SeqCst, MemoryOrder::Relaxed)) {
            // Item was dequeued by owner.
            return {};
        }
        return atomic_load(m_slots[static_cast<uint32_t>(tail % k_slot_count)]);
    }
    return {};
}

template <typename T, unsigned SlotCountShift>
bool WorkStealingQueue<T, SlotCountShift>::empty() const {
    return m_head.load(MemoryOrder::Acquire) <= m_tail.load(MemoryOrder::Acquire);
}

template <typename T, unsigned SlotCountShift>
uint64_t WorkStealingQueue<T, SlotCountShift>::size() const {
    int64_t head = m_head.load(MemoryOrder::Acquire);
    int64_t tail = m_tail.load(MemoryOrder::Acquire);
    return head >= tail ? static_cast<uint64_t>(head - tail) : 0u;
}

} // namespace vull
