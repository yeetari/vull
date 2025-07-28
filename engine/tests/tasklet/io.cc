#include <vull/platform/event.hh>
#include <vull/support/atomic.hh>
#include <vull/tasklet/functions.hh>
#include <vull/tasklet/future.hh>
#include <vull/tasklet/io.hh>
#include <vull/tasklet/scheduler.hh>
#include <vull/test/assertions.hh>
#include <vull/test/matchers.hh>
#include <vull/test/test.hh>

using namespace vull;
using namespace vull::test::matchers;

TEST_CASE(TaskletIo, NopAndThen) {
    tasklet::Scheduler scheduler(4, 64, false);
    scheduler.run([] {
        Atomic<bool> value;
        tasklet::submit_io_request<tasklet::NopRequest>()
            .and_then([&](tasklet::IoResult) {
            value.store(true);
        }).await();
        EXPECT_TRUE(value.load());
    });
}

TEST_CASE(TaskletIo, PollWaitEvent) {
    tasklet::Scheduler scheduler(4, 64, false);
    scheduler.run([] {
        Atomic<bool> first_value;
        Atomic<bool> second_value;
        platform::Event first_event;
        platform::Event second_event;
        tasklet::schedule([&] {
            tasklet::submit_io_request<tasklet::PollEventRequest>(first_event, false).await();
            EXPECT_TRUE(first_value.load());
            second_value.store(true);
            second_event.set();
        });
        first_value.store(true);
        first_event.set();
        tasklet::submit_io_request<tasklet::WaitEventRequest>(second_event).await();
        EXPECT_TRUE(second_value.load());
    });
}
