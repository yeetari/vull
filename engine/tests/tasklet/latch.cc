#include <vull/platform/semaphore.hh>
#include <vull/support/atomic.hh>
#include <vull/tasklet/functions.hh>
#include <vull/tasklet/latch.hh>
#include <vull/tasklet/scheduler.hh>
#include <vull/test/assertions.hh>
#include <vull/test/matchers.hh>
#include <vull/test/test.hh>

#include <stddef.h>

using namespace vull;
using namespace vull::test::matchers;

TEST_CASE(TaskletLatch, Arrive) {
    tasklet::Scheduler scheduler(4, 64, false);
    scheduler.run([] {
        Atomic<size_t> counter;
        tasklet::Latch latch(16);
        platform::Semaphore semaphore;
        for (size_t i = 0; i < 16; i++) {
            tasklet::schedule([&] {
                counter.fetch_add(1);
                latch.arrive();
                EXPECT_THAT(counter.load(), is(equal_to(16)));
                semaphore.post();
            });
        }
        for (size_t i = 0; i < 16; i++) {
            semaphore.wait();
        }
    });
}

TEST_CASE(TaskletLatch, CountDown) {
    // Use one thread to effectively guarantee a fail if wait() doesn't work.
    tasklet::Scheduler scheduler(1, 64, false);
    scheduler.run([] {
        Atomic<size_t> counter;
        tasklet::Latch latch(64);
        for (size_t i = 0; i < 64; i++) {
            tasklet::schedule([&] {
                counter.fetch_add(1);
                latch.count_down(1);
            });
        }
        latch.wait();
        EXPECT_THAT(counter.load(), is(equal_to(64)));
    });
}

TEST_CASE(TaskletLatch, TryWaitZero) {
    tasklet::Latch latch(0);
    EXPECT_TRUE(latch.try_wait());
}

TEST_CASE(TaskletLatch, TryWait) {
    tasklet::Scheduler scheduler(4, 64, false);
    scheduler.run([] {
        tasklet::Latch latch(3);
        EXPECT_FALSE(latch.try_wait());
        latch.count_down();
        EXPECT_FALSE(latch.try_wait());
        latch.count_down(2);
        EXPECT_TRUE(latch.try_wait());
    });
}
