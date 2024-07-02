#pragma once

#include <vull/container/array.hh>
#include <vull/support/atomic.hh>
#include <vull/support/optional.hh> // IWYU pragma: keep
#include <vull/support/utility.hh>

#include <stdint.h>

namespace vull {

// https://fzn.fr/readings/ppopp13.pdf
template <typename T, unsigned SlotCountShift = 10>
class WorkStealingQueue {
    // Don't wrap pointers in Optional.
    using RetType = conditional<is_ptr<T>, T, Optional<T>>;

    // Max size of the queue, should be a power of two to allow modulo operations to be transformed into cheaper ands.
    static constexpr int64_t k_slot_count = 1ul << SlotCountShift;
    Array<Atomic<T>, k_slot_count> m_slots{};
    Atomic<int64_t> m_head;
    Atomic<int64_t> m_tail;

public:
    [[nodiscard]] bool enqueue(T elem);
    RetType dequeue();
    RetType steal();

    bool empty() const;
    uint64_t size() const;
};

template <typename T, unsigned SlotCountShift>
[[nodiscard]] bool WorkStealingQueue<T, SlotCountShift>::enqueue(T elem) {
    int64_t head = m_head.load(vull::memory_order_relaxed);
    int64_t tail = m_tail.load(vull::memory_order_acquire);

    // Queue is already full.
    if (head - tail >= k_slot_count) {
        return false;
    }

    // Store element in slot and bump the head index.
    m_slots[static_cast<uint32_t>(head % k_slot_count)].store(elem);
    vull::atomic_thread_fence(vull::memory_order_release);
    m_head.store(head + 1, vull::memory_order_relaxed);
    return true;
}

template <typename T, unsigned SlotCountShift>
typename WorkStealingQueue<T, SlotCountShift>::RetType WorkStealingQueue<T, SlotCountShift>::dequeue() {
    int64_t index = m_head.fetch_sub(1, vull::memory_order_relaxed) - 1;
    vull::atomic_thread_fence(vull::memory_order_seq_cst);
    int64_t tail = m_tail.load(vull::memory_order_relaxed);

    // If the queue is empty, restore the head index and return nothing.
    if (tail > index) {
        m_head.store(index + 1, vull::memory_order_relaxed);
        return {};
    }

    T elem = m_slots[static_cast<uint32_t>(index % k_slot_count)].load();
    if (tail != index) {
        // This isn't the last element, so we can safely return it now.
        return elem;
    }

    // Else, there is only one element left and potential for it to be stolen.
    if (!m_tail.compare_exchange(tail, tail + 1, vull::memory_order_seq_cst, vull::memory_order_relaxed)) {
        // Failed race - last element was just stolen.
        m_head.store(index + 1, vull::memory_order_relaxed);
        return {};
    }
    m_head.store(index + 1, vull::memory_order_relaxed);
    return elem;
}

template <typename T, unsigned SlotCountShift>
typename WorkStealingQueue<T, SlotCountShift>::RetType WorkStealingQueue<T, SlotCountShift>::steal() {
    int64_t tail = m_tail.load(vull::memory_order_acquire);
    vull::atomic_thread_fence(vull::memory_order_seq_cst);
    int64_t head = m_head.load(vull::memory_order_acquire);

    // No available element to take.
    if (tail >= head) {
        return {};
    }

    T elem = m_slots[static_cast<uint32_t>(tail % k_slot_count)].load();
    if (!m_tail.compare_exchange(tail, tail + 1, vull::memory_order_seq_cst, vull::memory_order_relaxed)) {
        // Failed race - item was either dequeued by the queue owner or stolen by another thread.
        return {};
    }
    return elem;
}

template <typename T, unsigned SlotCountShift>
bool WorkStealingQueue<T, SlotCountShift>::empty() const {
    return m_head.load(vull::memory_order_acquire) <= m_tail.load(vull::memory_order_acquire);
}

template <typename T, unsigned SlotCountShift>
uint64_t WorkStealingQueue<T, SlotCountShift>::size() const {
    int64_t head = m_head.load(vull::memory_order_acquire);
    int64_t tail = m_tail.load(vull::memory_order_acquire);
    return head >= tail ? static_cast<uint64_t>(head - tail) : 0u;
}

} // namespace vull
