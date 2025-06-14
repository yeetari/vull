#include <vull/support/atomic.hh>
#include <vull/tasklet/latch.hh>
#include <vull/tasklet/scheduler.hh>
#include <vull/tasklet/tasklet.hh>
#include <vull/test/assertions.hh>
#include <vull/test/matchers.hh>
#include <vull/test/test.hh>

#include <stddef.h>

using namespace vull;
using namespace vull::test::matchers;

TEST_CASE(Tasklet, Counter) {
    Atomic<size_t> counter;
    Scheduler scheduler;
    scheduler.start([&] {
        for (size_t i = 0; i < 256; i++) {
            vull::schedule([&] {
                counter.fetch_add(1);
            });
        }
    });
    scheduler.join();
    EXPECT_THAT(counter.load(), is(equal_to(256)));
}

TEST_CASE(Tasklet, Latch) {
    Scheduler scheduler;
    scheduler.start([] {
        Atomic<size_t> counter;
        Latch latch(256);
        for (size_t i = 0; i < 256; i++) {
            vull::schedule([&] {
                counter.fetch_add(1);
                latch.count_down(1);
            });
        }
        latch.wait();
        EXPECT_THAT(counter.load(), is(equal_to(256)));
    });
}
