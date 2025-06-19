#pragma once

#include <vull/tasklet/tasklet.hh>

namespace vull {

/**
 * Adds the given tasklet to the scheduling queue. Blocks if the queue is full.
 *
 * @param tasklet the tasklet to schedule
 */
void schedule(Tasklet *tasklet);

/**
 * Adds the given callable to the scheduling queue. Blocks if the  queue is full. The callable can have an arbitrary
 * capture list but no parameters.
 *
 * @tparam F the callable type
 * @tparam Size the tasklet stack size to use
 * @param callable the callable to schedule
 */
template <typename F, TaskletSize Size = TaskletSize::Normal>
void schedule(F &&callable) {
    auto *tasklet = Tasklet::create<Size>();
    tasklet->set_callable(vull::forward<F>(callable));
    schedule(tasklet);
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
