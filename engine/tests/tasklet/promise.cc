#include <vull/support/atomic.hh>
#include <vull/tasklet/promise.hh>
#include <vull/tasklet/scheduler.hh>
#include <vull/tasklet/tasklet.hh>
#include <vull/test/assertions.hh>
#include <vull/test/matchers.hh>
#include <vull/test/test.hh>

#include <stdint.h>

using namespace vull;
using namespace vull::test::matchers;

TEST_CASE(TaskletPromise, Void) {
    tasklet::Scheduler scheduler(4, 64, false);
    scheduler.run([] {
        Atomic<uint32_t> counter;
        tasklet::Promise<void> promise;
        auto *first_tasklet = new tasklet::PromisedTasklet([&] {
            counter.fetch_add(1);
        });
        auto *second_tasklet = new tasklet::PromisedTasklet([&] {
            counter.fetch_add(1);
        });
        first_tasklet->add_ref();
        second_tasklet->add_ref();
        EXPECT_TRUE(promise.add_waiter(first_tasklet));
        promise.fulfill();
        EXPECT_FALSE(promise.add_waiter(second_tasklet));
        first_tasklet->wait();
        EXPECT_THAT(counter.load(), is(equal_to(1)));
        promise.wake_on_fulfillment(second_tasklet);
        second_tasklet->wait();
        EXPECT_THAT(counter.load(), is(equal_to(2)));
        first_tasklet->sub_ref();
        second_tasklet->sub_ref();
    });
}

TEST_CASE(TaskletPromise, IsFulfilled) {
    tasklet::Promise<void> promise;
    EXPECT_FALSE(promise.is_fulfilled());
    promise.fulfill();
    EXPECT_TRUE(promise.is_fulfilled());
}

TEST_CASE(TaskletPromise, Reset) {
    tasklet::Promise<void> promise;
    promise.fulfill();
    promise.reset();
    EXPECT_FALSE(promise.is_fulfilled());
}
