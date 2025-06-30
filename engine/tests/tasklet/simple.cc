#include <vull/support/atomic.hh>
#include <vull/tasklet/functions.hh>
#include <vull/tasklet/future.hh>
#include <vull/tasklet/latch.hh>
#include <vull/tasklet/scheduler.hh>
#include <vull/test/assertions.hh>
#include <vull/test/matchers.hh>
#include <vull/test/test.hh>

#include <stddef.h>

using namespace vull;
using namespace vull::test::matchers;

TEST_CASE(Tasklet, Counter) {
    Atomic<size_t> counter;
    tasklet::Scheduler scheduler;
    scheduler.run([&] {
        for (size_t i = 0; i < 256; i++) {
            tasklet::schedule([&] {
                counter.fetch_add(1);
            });
        }
    });
    EXPECT_THAT(counter.load(), is(equal_to(256)));
}

TEST_CASE(Tasklet, Latch) {
    tasklet::Scheduler scheduler;
    scheduler.run([] {
        Atomic<size_t> counter;
        tasklet::Latch latch(256);
        for (size_t i = 0; i < 256; i++) {
            tasklet::schedule([&] {
                counter.fetch_add(1);
                latch.count_down(1);
            });
        }
        latch.wait();
        EXPECT_THAT(counter.load(), is(equal_to(256)));
    });
}

TEST_CASE(Tasklet, SchedulerRun) {
    tasklet::Scheduler scheduler;
    auto value = scheduler.run([] {
        return 5;
    });
    EXPECT_THAT(value, is(equal_to(5)));
}

TEST_CASE(Tasklet, InTaskletContext) {
    EXPECT_FALSE(tasklet::in_tasklet_context());
    tasklet::Scheduler scheduler;
    scheduler.run([] {
        tasklet::schedule([] {
            EXPECT_TRUE(tasklet::in_tasklet_context());
        }).await();
        EXPECT_TRUE(tasklet::in_tasklet_context());
    });
}
