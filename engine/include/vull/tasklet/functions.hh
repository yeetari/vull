#pragma once

#include <vull/tasklet/future.hh>
#include <vull/tasklet/tasklet.hh>

namespace vull {

/**
 * Adds the given tasklet to the scheduling queue. Blocks if the queue is full.
 *
 * @param tasklet the tasklet to schedule
 */
void schedule(Tasklet *tasklet);

/**
 * Adds the given callable to the scheduling queue and returns a Future associated with its completion. Blocks if the
 * queue is full. The callable can have an arbitrary capture list but no parameters.
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
 * Suspends the current tasklet's execution until rescheduled by another tasklet.
 */
void suspend();

/**
 * Yields the current tasklet's execution to the scheduler. The current tasklet will be rescheduled automatically.
 */
void yield();

} // namespace vull
