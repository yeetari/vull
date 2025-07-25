#pragma once

#include <vull/support/function.hh>
#include <vull/support/shared_ptr.hh>
#include <vull/support/utility.hh>
#include <vull/tasklet/promise.hh>
#include <vull/tasklet/tasklet.hh>

namespace vull::tasklet {

/**
 * @brief A class which represents the result of an asynchronous execution of an operation within a tasklet context, for
 * example the execution of a tasklet or the execution of an IO request. Another tasklet can wait on the future's
 * completion and retrieve its result when ready.
 *
 * @ingroup Tasklet
 */
template <typename T>
class Future {
    friend SharedPromise<T>;

private:
    SharedPtr<SharedPromise<T>> m_promise;

public:
    /**
     * @brief Default constructs the future with no underlying promise.
     */
    Future() = default;

    /**
     * @brief Constructs the future with the given promise.
     */
    Future(SharedPtr<SharedPromise<T>> &&promise) : m_promise(vull::move(promise)) {}

    /**
     * @brief Suspends the calling tasklet until the future is complete and returns its result.
     */
    T await() const;

    /**
     * @brief Schedules the given callable to run on completion of this future and returns another future associated
     * with its completion. If this future's result is not void, it is passed to the callable as the only argument.
     *
     * The callable may return any type, including void. The returned future can be chained with additional `and_then()`
     * calls.
     *
     * @tparam F the callable type
     * @param callable the callable to schedule on completion
     * @return a Future which stores the result of the callable's invocation
     */
    template <typename F>
    auto and_then(F &&callable);

    /**
     * @brief Returns true if the `Promise` underlying the future has been fulfilled.
     *
     * This function does not provide any memory ordering guarantees and a call to `await()` should still be made, in
     * which case `await()` will not block if the future is complete.
     */
    bool is_complete() const;

    /**
     * @brief Returns true if the future is holding a valid reference to a `Promise`.
     */
    bool is_valid() const;
};

template <typename T>
Future(T) -> Future<T>;

template <typename T>
T Future<T>::await() const {
    m_promise->wait();
    if constexpr (!vull::is_same<T, void>) {
        // Move the fulfilled value out of the promise.
        return vull::move(m_promise->value());
    }
}

template <typename T>
template <typename F>
auto Future<T>::and_then(F &&callable) {
    using R = FunctionTraits<F>::result_type;
    auto *tasklet = new PromisedTasklet([*this, callable = vull::move(callable)] mutable {
        if constexpr (vull::is_same<T, void>) {
            await();
            if constexpr (vull::is_same<R, void>) {
                callable();
            } else {
                return callable();
            }
        } else {
            if constexpr (vull::is_same<R, void>) {
                callable(await());
            } else {
                return callable(await());
            }
        }
    });
    Future<R> future(vull::adopt_shared(tasklet));
    m_promise->wake_on_fulfillment(tasklet);
    return future;
}

template <typename T>
bool Future<T>::is_complete() const {
    return m_promise->is_fulfilled();
}

template <typename T>
bool Future<T>::is_valid() const {
    return !m_promise.is_null();
}

} // namespace vull::tasklet
