#pragma once

#include <vull/support/Array.hh>
#include <vull/support/Utility.hh>

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
T atomic_load(T &ptr, MemoryOrder order = MemoryOrder::Relaxed) {
    // NOLINTNEXTLINE: buffer doesn't need to be initialised.
    alignas(T) Array<char, sizeof(T)> buffer;
    auto *buf_ptr = reinterpret_cast<T *>(buffer.data());
    __atomic_load(&ptr, buf_ptr, static_cast<int>(order));
    return move(*buf_ptr);
}

template <typename T>
void atomic_store(T &ptr, T &&val, MemoryOrder order = MemoryOrder::Relaxed) {
    __atomic_store(&ptr, &val, static_cast<int>(order));
}

template <typename T>
concept SimpleAtomic = requires(T t) {
    __atomic_load_n(&t, __ATOMIC_RELAXED);
};

template <SimpleAtomic T>
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

template <SimpleAtomic T>
bool Atomic<T>::compare_exchange(T &expected, T desired, MemoryOrder success_order,
                                 MemoryOrder failure_order) volatile {
    return __atomic_compare_exchange_n(&m_value, &expected, desired, false, static_cast<int>(success_order),
                                       static_cast<int>(failure_order));
}

template <SimpleAtomic T>
T Atomic<T>::exchange(T desired, MemoryOrder order) volatile {
    return __atomic_exchange_n(&m_value, desired, static_cast<int>(order));
}

template <SimpleAtomic T>
T Atomic<T>::fetch_add(T value, MemoryOrder order) volatile {
    return __atomic_fetch_add(&m_value, value, static_cast<int>(order));
}

template <SimpleAtomic T>
T Atomic<T>::fetch_sub(T value, MemoryOrder order) volatile {
    return __atomic_fetch_sub(&m_value, value, static_cast<int>(order));
}

template <SimpleAtomic T>
T Atomic<T>::load(MemoryOrder order) const volatile {
    return __atomic_load_n(&m_value, static_cast<int>(order));
}

template <SimpleAtomic T>
void Atomic<T>::store(T value, MemoryOrder order) volatile {
    __atomic_store_n(&m_value, value, static_cast<int>(order));
}

} // namespace vull
