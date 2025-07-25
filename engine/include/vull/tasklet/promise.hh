#pragma once

#include <vull/support/atomic.hh>
#include <vull/support/utility.hh>

#include <stdint.h>

namespace vull::tasklet {

class Tasklet;

/**
 * @brief The base promise type which contains the wait list logic.
 */
class PromiseBase {
    Atomic<Tasklet *> m_wait_list;

protected:
    /**
     * @brief Unsuspends and reschedules all of the tasklets waiting on this promise.
     */
    void wake_all();

public:
    /**
     * @brief Adds the given tasklet to the wait list of this promise. Returns false if the promise has already been
     * fulfilled.
     *
     * @param tasklet the tasklet to add to the wait list
     * @return true if the tasklet was added to the wait list; false otherwise
     */
    bool add_waiter(Tasklet *tasklet);

    /**
     * @brief Returns true if the promise has been fulfilled.
     */
    bool is_fulfilled() const;

    /**
     * @brief Resets the fulfilled promise.
     */
    void reset();

    /**
     * @brief Schedules the given tasklet to be unsuspended upon the promise's fulfillment.
     *
     * If the promise has already been fulfilled, the tasklet will be rescheduled immediately.
     *
     * @param tasklet the tasklet to wake
     */
    void wake_on_fulfillment(Tasklet *tasklet);

    /**
     * @brief Suspends the calling tasklet until the promise has been fulfilled.
     */
    void wait();
};

/**
 * @brief A typed promise which can be fulfilled with an object of the given type.
 *
 * @tparam T the type of the fulfilled object
 * @ingroup Tasklet
 */
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

    /**
     * @brief Fulfills the promise with the given value and unsuspends all waiting tasklets.
     *
     * @param value the value to fulfill the promise with
     */
    void fulfill(const T &value);

    /**
     * @copydoc fulfill(const T &)
     */
    void fulfill(T &&value);

    T &value() { return m_storage.get(); }
    const T &value() const { return m_storage.get(); }
};

/**
 * @brief An untyped promise.
 *
 * @ingroup Tasklet
 */
template <>
class Promise<void> : public PromiseBase {
public:
    /**
     * @brief Fulfills the promise and unsuspends all waiting tasklets.
     */
    void fulfill() { wake_all(); }
};

/**
 * @brief An intrusively reference counted `Promise` which works with `SharedPtr`.
 *
 * This is the type held by `Future`.
 *
 * @ingroup Tasklet
 */
template <typename T>
class SharedPromise : public Promise<T> {
    mutable Atomic<uint32_t> m_ref_count{0};

public:
    SharedPromise() = default;
    SharedPromise(const SharedPromise &) = delete;
    SharedPromise(SharedPromise &&) = delete;
    virtual ~SharedPromise() = default;

    SharedPromise &operator=(const SharedPromise &) = delete;
    SharedPromise &operator=(SharedPromise &&) = delete;

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
        delete this;
    }
}

} // namespace vull::tasklet
