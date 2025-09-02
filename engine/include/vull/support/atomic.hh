#pragma once

#include <vull/container/array.hh>
#include <vull/support/enum.hh>
#include <vull/support/utility.hh>

namespace vull {

inline constexpr int memory_order_relaxed = __ATOMIC_RELAXED;
inline constexpr int memory_order_acquire = __ATOMIC_ACQUIRE;
inline constexpr int memory_order_release = __ATOMIC_RELEASE;
inline constexpr int memory_order_acq_rel = __ATOMIC_ACQ_REL;
inline constexpr int memory_order_seq_cst = __ATOMIC_SEQ_CST;

inline void atomic_thread_fence(int order) {
    __atomic_thread_fence(order);
}

template <typename T>
T atomic_load(T &ptr, int order = memory_order_relaxed) {
    AlignedStorage<T> storage;
    __atomic_load(&ptr, &storage.get(), order);
    return vull::move(storage.get());
}

template <typename T>
void atomic_store(T &ptr, T &&val, int order = memory_order_relaxed) {
    __atomic_store(&ptr, &val, order);
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
concept SimpleAtomic = requires(atomic_storage_t<T> t) { __atomic_load_n(&t, __ATOMIC_RELAXED); };

template <SimpleAtomic T>
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

    T cmpxchg(T expected, T desired, int success_order = memory_order_relaxed,
              int failure_order = memory_order_relaxed) volatile;
    T cmpxchg_weak(T expected, T desired, int success_order = memory_order_relaxed,
                   int failure_order = memory_order_relaxed) volatile;
    bool compare_exchange(T &expected, T desired, int success_order = memory_order_relaxed,
                          int failure_order = memory_order_relaxed) volatile;
    bool compare_exchange_weak(T &expected, T desired, int success_order = memory_order_relaxed,
                               int failure_order = memory_order_relaxed) volatile;
    T exchange(T desired, int order = memory_order_relaxed) volatile;
    T fetch_add(T value, int order = memory_order_relaxed) volatile;
    T fetch_sub(T value, int order = memory_order_relaxed) volatile;
    T fetch_and(T value, int order = memory_order_relaxed) volatile;
    T fetch_or(T value, int order = memory_order_relaxed) volatile;
    T fetch_xor(T value, int order = memory_order_relaxed) volatile;
    T load(int order = memory_order_relaxed) const volatile;
    void store(T value, int order = memory_order_relaxed) volatile;
    storage_t *raw_ptr() { return &m_value; }
};

template <SimpleAtomic T>
T Atomic<T>::cmpxchg(T expected, T desired, int success_order, int failure_order) volatile {
    auto exp = storage_t(expected);
    __atomic_compare_exchange_n(&m_value, &exp, storage_t(desired), false, success_order, failure_order);
    return T(exp);
}

template <SimpleAtomic T>
T Atomic<T>::cmpxchg_weak(T expected, T desired, int success_order, int failure_order) volatile {
    auto exp = storage_t(expected);
    __atomic_compare_exchange_n(&m_value, &exp, storage_t(desired), true, success_order, failure_order);
    return T(exp);
}

template <SimpleAtomic T>
bool Atomic<T>::compare_exchange(T &expected, T desired, int success_order, int failure_order) volatile {
    auto exp = storage_t(expected);
    bool success = __atomic_compare_exchange_n(&m_value, &exp, storage_t(desired), false, success_order, failure_order);
    expected = T(exp);
    return success;
}

template <SimpleAtomic T>
bool Atomic<T>::compare_exchange_weak(T &expected, T desired, int success_order, int failure_order) volatile {
    auto exp = storage_t(expected);
    bool success = __atomic_compare_exchange_n(&m_value, &exp, storage_t(desired), true, success_order, failure_order);
    expected = T(exp);
    return success;
}

template <SimpleAtomic T>
T Atomic<T>::exchange(T desired, int order) volatile {
    return T(__atomic_exchange_n(&m_value, storage_t(desired), order));
}

template <SimpleAtomic T>
T Atomic<T>::fetch_add(T value, int order) volatile {
    return __atomic_fetch_add(&m_value, value, order);
}

template <SimpleAtomic T>
T Atomic<T>::fetch_sub(T value, int order) volatile {
    return __atomic_fetch_sub(&m_value, value, order);
}

template <SimpleAtomic T>
T Atomic<T>::fetch_and(T value, int order) volatile {
    return __atomic_fetch_and(&m_value, value, order);
}

template <SimpleAtomic T>
T Atomic<T>::fetch_or(T value, int order) volatile {
    return __atomic_fetch_or(&m_value, value, order);
}

template <SimpleAtomic T>
T Atomic<T>::fetch_xor(T value, int order) volatile {
    return __atomic_fetch_xor(&m_value, value, order);
}

template <SimpleAtomic T>
T Atomic<T>::load(int order) const volatile {
    return T(__atomic_load_n(&m_value, order));
}

template <SimpleAtomic T>
void Atomic<T>::store(T value, int order) volatile {
    __atomic_store_n(&m_value, storage_t(value), order);
}

} // namespace vull
