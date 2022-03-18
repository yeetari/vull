#pragma once

namespace vull {

enum class MemoryOrder {
    Relaxed = __ATOMIC_RELAXED,
    Consume = __ATOMIC_CONSUME,
    Acquire = __ATOMIC_ACQUIRE,
    Release = __ATOMIC_RELEASE,
    AcqRel = __ATOMIC_ACQ_REL,
    SeqCst = __ATOMIC_SEQ_CST,
};

template <typename T>
class Atomic {
    T m_value{};

public:
    Atomic() = default;
    Atomic(T value) : m_value(value) {}
    Atomic(const Atomic &) = delete;
    Atomic(Atomic &&) = delete;
    ~Atomic() = default;

    Atomic &operator=(const Atomic &) = delete;
    Atomic &operator=(Atomic &&) = delete;

    bool compare_exchange(T &expected, T desired, MemoryOrder success_order, MemoryOrder failure_order) volatile;
    T exchange(T desired, MemoryOrder order) volatile;
    T fetch_add(T value, MemoryOrder order) volatile;
    T fetch_sub(T value, MemoryOrder order) volatile;
    T load(MemoryOrder order) const volatile;
    void store(T value, MemoryOrder order) volatile;
};

template <typename T>
bool Atomic<T>::compare_exchange(T &expected, T desired, MemoryOrder success_order,
                                 MemoryOrder failure_order) volatile {
    return __atomic_compare_exchange_n(&m_value, &expected, desired, false, static_cast<int>(success_order),
                                       static_cast<int>(failure_order));
}

template <typename T>
T Atomic<T>::exchange(T desired, MemoryOrder order) volatile {
    return __atomic_exchange_n(&m_value, desired, static_cast<int>(order));
}

template <typename T>
T Atomic<T>::fetch_add(T value, MemoryOrder order) volatile {
    return __atomic_fetch_add(&m_value, value, static_cast<int>(order));
}

template <typename T>
T Atomic<T>::fetch_sub(T value, MemoryOrder order) volatile {
    return __atomic_fetch_sub(&m_value, value, static_cast<int>(order));
}

template <typename T>
T Atomic<T>::load(MemoryOrder order) const volatile {
    return __atomic_load_n(&m_value, static_cast<int>(order));
}

template <typename T>
void Atomic<T>::store(T value, MemoryOrder order) volatile {
    __atomic_store_n(&m_value, value, static_cast<int>(order));
}

} // namespace vull
