#pragma once

#include <vull/support/function.hh>
#include <vull/support/shared_ptr.hh>
#include <vull/support/utility.hh>
#include <vull/tasklet/promise.hh>
#include <vull/tasklet/tasklet.hh>

namespace vull::tasklet {

/**
 * @brief A class which represents the result of an asynchronous tasklet execution. Another tasklet can wait on a
 * tasklet's completion and retrieve its result when ready.
 */
template <typename T>
class Future {
    friend SharedPromise<T>;

private:
    SharedPtr<SharedPromise<T>> m_promise;

public:
    Future() = default;
    Future(SharedPtr<SharedPromise<T>> &&promise) : m_promise(vull::move(promise)) {}

    T await() const;

    template <typename F>
    auto and_then(F &&callable);

    bool is_complete() const;
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
