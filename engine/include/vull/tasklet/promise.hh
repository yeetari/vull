#pragma once

#include <vull/support/atomic.hh>
#include <vull/support/utility.hh>

#include <stdint.h>

namespace vull::tasklet {

class Tasklet;

class PromiseBase {
    Atomic<Tasklet *> m_wait_list;

protected:
    void wake_all();

public:
    bool add_waiter(Tasklet *tasklet);
    bool is_fulfilled() const;
    void reset();
    void wake_on_fulfillment(Tasklet *tasklet);
    void wait();
};

template <typename T>
class Promise : public PromiseBase {
    AlignedStorage<T> m_storage{};

public:
    Promise() = default;
    Promise(const Promise &) = delete;
    Promise(Promise &&) = delete;
    ~Promise();

    Promise &operator=(const Promise &) = delete;
    Promise &operator=(Promise &&) = delete;

    void fulfill(const T &value);
    void fulfill(T &&value);

    T &value() { return m_storage.get(); }
    const T &value() const { return m_storage.get(); }
};

template <>
class Promise<void> : public PromiseBase {
public:
    void fulfill() { wake_all(); }
};

template <typename T>
class SharedPromise : public Promise<T> {
    mutable Atomic<uint32_t> m_ref_count{0};

public:
    void add_ref() const;
    void sub_ref() const;
};

template <typename T>
Promise<T>::~Promise() {
    if constexpr (!vull::is_trivially_destructible<T>) {
        if (is_fulfilled()) {
            m_storage.release();
        }
    }
}

template <typename T>
void Promise<T>::fulfill(const T &value) {
    m_storage.set(value);
    wake_all();
}

template <typename T>
void Promise<T>::fulfill(T &&value) {
    m_storage.set(vull::move(value));
    wake_all();
}

template <typename T>
void SharedPromise<T>::add_ref() const {
    m_ref_count.fetch_add(1, vull::memory_order_relaxed);
}

template <typename T>
void SharedPromise<T>::sub_ref() const {
    if (m_ref_count.fetch_sub(1, vull::memory_order_acq_rel) == 1) {
        // Use operator delete to avoid sized deallocation.
        // NOLINTNEXTLINE
        ::operator delete(const_cast<SharedPromise<T> *>(this));
    }
}

} // namespace vull::tasklet
