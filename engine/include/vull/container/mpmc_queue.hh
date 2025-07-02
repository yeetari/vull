#pragma once

#include <vull/container/array.hh>
#include <vull/support/atomic.hh>
#include <vull/support/optional.hh>
#include <vull/support/utility.hh>

#include <stdint.h>

namespace vull {

/**
 * @brief A lock-free multi-producer multi-consumer queue with FIFO ordering.
 *
 * Algorithm from: https://github.com/rigtorp/MPMCQueue
 */
template <TriviallyCopyable T, uint32_t SlotCountShift = 10>
class MpmcQueue {
    // Don't wrap pointers in Optional.
    using dequeued_type = vull::conditional<vull::is_ptr<T>, T, Optional<T>>;

    static constexpr uint32_t k_slot_shift = SlotCountShift;
    static constexpr uint32_t k_slot_count = 1u << k_slot_shift;

private:
    Array<Atomic<T>, k_slot_count> m_slots{};
    Array<Atomic<uint32_t>, k_slot_count> m_turns{};
    Atomic<uint32_t> m_head;
    Atomic<uint32_t> m_tail;

public:
    [[nodiscard]] bool enqueue(T value);
    dequeued_type dequeue();

    [[nodiscard]] bool empty() const;
    uint32_t size() const;
};

template <TriviallyCopyable T, uint32_t SlotCountShift>
bool MpmcQueue<T, SlotCountShift>::enqueue(T value) {
    uint32_t head = m_head.load(vull::memory_order_acquire);
    while (true) {
        auto &slot = m_slots[head % k_slot_count];
        auto &turn = m_turns[head % k_slot_count];
        if ((head / k_slot_count) * 2 == turn.load(vull::memory_order_acquire)) {
            if (m_head.compare_exchange(head, head + 1)) {
                slot.store(value);
                turn.store((head / k_slot_count) * 2 + 1, vull::memory_order_release);
                return true;
            }
        } else {
            uint32_t old_head = vull::exchange(head, m_head.load(vull::memory_order_acquire));
            if (head == old_head) {
                return false;
            }
        }
    }
}

template <TriviallyCopyable T, uint32_t SlotCountShift>
MpmcQueue<T, SlotCountShift>::dequeued_type MpmcQueue<T, SlotCountShift>::dequeue() {
    uint32_t tail = m_tail.load(vull::memory_order_acquire);
    while (true) {
        auto &slot = m_slots[tail % k_slot_count];
        auto &turn = m_turns[tail % k_slot_count];
        if ((tail / k_slot_count) * 2 + 1 == turn.load(vull::memory_order_acquire)) {
            if (m_tail.compare_exchange(tail, tail + 1)) {
                auto value = slot.load();
                turn.store((tail / k_slot_count) * 2 + 2, vull::memory_order_release);
                return value;
            }
        } else {
            uint32_t old_tail = vull::exchange(tail, m_tail.load(vull::memory_order_acquire));
            if (tail == old_tail) {
                return {};
            }
        }
    }
}

template <TriviallyCopyable T, uint32_t SlotCountShift>
bool MpmcQueue<T, SlotCountShift>::empty() const {
    return m_head.load(vull::memory_order_relaxed) <= m_tail.load(vull::memory_order_relaxed);
}

template <TriviallyCopyable T, uint32_t SlotCountShift>
uint32_t MpmcQueue<T, SlotCountShift>::size() const {
    uint32_t head = m_head.load(vull::memory_order_relaxed);
    uint32_t tail = m_tail.load(vull::memory_order_relaxed);
    return head >= tail ? head - tail : 0u;
}

} // namespace vull
