#pragma once

#include <vull/support/Array.hh>
#include <vull/support/Enum.hh>
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

inline void atomic_thread_fence(MemoryOrder order) {
    __atomic_thread_fence(static_cast<int>(order));
}

template <typename T>
T atomic_load(T &ptr, MemoryOrder order = MemoryOrder::Relaxed) {
    AlignedStorage<T> storage;
    __atomic_load(&ptr, &storage.get(), static_cast<int>(order));
    return move(storage.get());
}

template <typename T>
void atomic_store(T &ptr, T &&val, MemoryOrder order = MemoryOrder::Relaxed) {
    __atomic_store(&ptr, &val, static_cast<int>(order));
}

template <typename T>
struct AtomicStorageType {
    using type = T;
};
template <Enum T>
struct AtomicStorageType<T> {
    using type = underlying_type<T>;
};

template <typename T>
using atomic_storage_t = typename AtomicStorageType<T>::type;

template <typename T>
concept SimpleAtomic = requires(atomic_storage_t<T> t) {
    __atomic_load_n(&t, __ATOMIC_RELAXED);
};

template <SimpleAtomic T, MemoryOrder Order = MemoryOrder::Relaxed>
class Atomic {
    using storage_t = atomic_storage_t<T>;
    storage_t m_value{};

public:
    Atomic() = default;
    Atomic(T value) : m_value(storage_t(value)) {}
    Atomic(const Atomic &) = delete;
    Atomic(Atomic &&) = delete;
    ~Atomic() = default;

    Atomic &operator=(const Atomic &) = delete;
    Atomic &operator=(Atomic &&) = delete;

    T cmpxchg(T expected, T desired, MemoryOrder success_order = Order, MemoryOrder failure_order = Order) volatile;
    T cmpxchg_weak(T expected, T desired, MemoryOrder success_order = Order,
                   MemoryOrder failure_order = Order) volatile;
    bool compare_exchange(T &expected, T desired, MemoryOrder success_order = Order,
                          MemoryOrder failure_order = Order) volatile;
    bool compare_exchange_weak(T &expected, T desired, MemoryOrder success_order = Order,
                               MemoryOrder failure_order = Order) volatile;
    T exchange(T desired, MemoryOrder order = Order) volatile;
    T fetch_add(T value, MemoryOrder order = Order) volatile;
    T fetch_sub(T value, MemoryOrder order = Order) volatile;
    T load(MemoryOrder order = Order) const volatile;
    void store(T value, MemoryOrder order = Order) volatile;
    storage_t *raw_ptr() { return &m_value; }
};

template <SimpleAtomic T, MemoryOrder Order>
T Atomic<T, Order>::cmpxchg(T expected, T desired, MemoryOrder success_order, MemoryOrder failure_order) volatile {
    auto exp = storage_t(expected);
    __atomic_compare_exchange_n(&m_value, &exp, storage_t(desired), false, static_cast<int>(success_order),
                                static_cast<int>(failure_order));
    return T(exp);
}

template <SimpleAtomic T, MemoryOrder Order>
T Atomic<T, Order>::cmpxchg_weak(T expected, T desired, MemoryOrder success_order, MemoryOrder failure_order) volatile {
    auto exp = storage_t(expected);
    __atomic_compare_exchange_n(&m_value, &exp, storage_t(desired), true, static_cast<int>(success_order),
                                static_cast<int>(failure_order));
    return T(exp);
}

template <SimpleAtomic T, MemoryOrder Order>
bool Atomic<T, Order>::compare_exchange(T &expected, T desired, MemoryOrder success_order,
                                        MemoryOrder failure_order) volatile {
    auto exp = storage_t(expected);
    bool success = __atomic_compare_exchange_n(&m_value, &exp, storage_t(desired), false,
                                               static_cast<int>(success_order), static_cast<int>(failure_order));
    expected = T(exp);
    return success;
}

template <SimpleAtomic T, MemoryOrder Order>
bool Atomic<T, Order>::compare_exchange_weak(T &expected, T desired, MemoryOrder success_order,
                                             MemoryOrder failure_order) volatile {
    auto exp = storage_t(expected);
    bool success = __atomic_compare_exchange_n(&m_value, &exp, storage_t(desired), true,
                                               static_cast<int>(success_order), static_cast<int>(failure_order));
    expected = T(exp);
    return success;
}

template <SimpleAtomic T, MemoryOrder Order>
T Atomic<T, Order>::exchange(T desired, MemoryOrder order) volatile {
    return T(__atomic_exchange_n(&m_value, storage_t(desired), static_cast<int>(order)));
}

template <SimpleAtomic T, MemoryOrder Order>
T Atomic<T, Order>::fetch_add(T value, MemoryOrder order) volatile {
    return __atomic_fetch_add(&m_value, value, static_cast<int>(order));
}

template <SimpleAtomic T, MemoryOrder Order>
T Atomic<T, Order>::fetch_sub(T value, MemoryOrder order) volatile {
    return __atomic_fetch_sub(&m_value, value, static_cast<int>(order));
}

template <SimpleAtomic T, MemoryOrder Order>
T Atomic<T, Order>::load(MemoryOrder order) const volatile {
    return T(__atomic_load_n(&m_value, static_cast<int>(order)));
}

template <SimpleAtomic T, MemoryOrder Order>
void Atomic<T, Order>::store(T value, MemoryOrder order) volatile {
    __atomic_store_n(&m_value, storage_t(value), static_cast<int>(order));
}

} // namespace vull
