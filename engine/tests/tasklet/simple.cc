#include <vull/tasklet/functions.hh>
#include <vull/tasklet/future.hh>
#include <vull/tasklet/scheduler.hh>
#include <vull/test/assertions.hh>
#include <vull/test/matchers.hh>
#include <vull/test/test.hh>

using namespace vull;
using namespace vull::test::matchers;

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
    EXPECT_FALSE(tasklet::in_tasklet_context());
}
