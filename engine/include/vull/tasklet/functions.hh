#pragma once

#include <vull/support/utility.hh>
#include <vull/tasklet/future.hh>
#include <vull/tasklet/io.hh>
#include <vull/tasklet/tasklet.hh>

namespace vull::tasklet {

/**
 * @brief Returns true if the calling thread is in a tasklet context.
 */
bool in_tasklet_context();

/**
 * @brief Adds the given tasklet to the scheduling queue. Blocks if the queue is full.
 *
 * @param tasklet the tasklet to schedule
 */
void schedule(Tasklet *tasklet);

/**
 * @brief Adds the given callable to the scheduling queue and returns a Future associated with its completion. Blocks if
 * the queue is full. The callable can have an arbitrary capture list but no parameters.
 *
 * @tparam F the callable type
 * @tparam Size the tasklet stack size to use
 * @param callable the callable to schedule
 * @return a Future which stores the result of the callable's invocation
 */
template <typename F, TaskletSize Size = TaskletSize::Normal>
auto schedule(F &&callable) {
    auto *tasklet = Tasklet::create<Size>();
    auto promise = tasklet->set_callable(vull::forward<F>(callable));
    schedule(tasklet);
    return Future(vull::move(promise));
}

/**
 * @brief Adds the given IoRequest to the IO queue. Blocks if the queue is full.
 *
 * @param request the IoRequest to submit
 */
void submit_io_request(SharedPtr<IoRequest> request);

/**
 * @brief Constructs and submits a typed IO request to the IO queue and returns a Future associated with its completion.
 * Blocks if the queue is full.
 *
 * This function creates a new IO request of type T with the given arguments and adds it to the IO queue. If the queue
 * is full, the function will yield the current tasklet to the scheduler until space is available. The returned Future
 * keeps the request alive and allows the caller or another tasklet to wait for and retrieve the result of the IO
 * operation.
 *
 * @tparam T the type of IO request to create; must be derived from IoRequest
 * @tparam Args the argument types to pass to T's constructor
 * @param args the arguments with which to construct the request
 * @return a Future<IoResult> representing the result of the IO operation
 */
template <DerivedFrom<IoRequest> T, typename... Args>
Future<IoResult> submit_io_request(Args &&...args) {
    SharedPtr<T> request(new T(vull::forward<Args>(args)...));
    submit_io_request(request);
    return Future<IoResult>(vull::move(request));
}

/**
 * @brief Suspends the current tasklet's execution until rescheduled by another tasklet.
 */
void suspend();

/**
 * @brief Yields the current tasklet's execution to the scheduler. The current tasklet will be rescheduled
 * automatically.
 */
void yield();

} // namespace vull::tasklet
