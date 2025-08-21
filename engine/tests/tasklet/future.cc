#include <vull/support/atomic.hh>
#include <vull/support/shared_ptr.hh>
#include <vull/support/utility.hh>
#include <vull/tasklet/functions.hh>
#include <vull/tasklet/future.hh>
#include <vull/tasklet/promise.hh>
#include <vull/tasklet/scheduler.hh>
#include <vull/test/assertions.hh>
#include <vull/test/matchers.hh>
#include <vull/test/move_tester.hh>
#include <vull/test/test.hh>

#include <stddef.h>

using namespace vull;
using namespace vull::test::matchers;

TEST_CASE(TaskletFuture, AwaitVoid) {
    tasklet::Scheduler scheduler(4, 64, false);
    scheduler.run([] {
        Atomic<size_t> counter;
        auto future = tasklet::schedule([&] {
            counter.fetch_add(1);
        });
        future.await();
        EXPECT_THAT(counter.load(), is(equal_to(1)));
    });
}

TEST_CASE(TaskletFuture, AwaitTrivial) {
    tasklet::Scheduler scheduler(4, 64, false);
    scheduler.run([] {
        auto future = tasklet::schedule([] {
            return 5;
        });
        EXPECT_THAT(future.await(), is(equal_to(5)));
    });
}

TEST_CASE(TaskletFuture, AwaitMove) {
    tasklet::Scheduler scheduler(4, 64, false);
    scheduler.run([] {
        size_t destruct_count = 0;
        {
            auto future = tasklet::schedule([&] {
                return test::MoveTester(destruct_count);
            });
            {
                auto tester = future.await();
                EXPECT_FALSE(tester.is_empty());

                // Destruct count should remain 0 as promise is still referenced.
                EXPECT_THAT(destruct_count, is(equal_to(0)));
            }
            // Should increase to 1 after the awaited object goes out of scope.
            EXPECT_THAT(destruct_count, is(equal_to(1)));
        }
        // Should remain at 1 after the promise is destroyed.
        EXPECT_THAT(destruct_count, is(equal_to(1)));
    });
}

TEST_CASE(TaskletFuture, AwaitThread) {
    tasklet::Scheduler scheduler(4, 64, false);
    scheduler.setup_thread();

    auto future = tasklet::schedule([&] {
        return 5;
    });
    EXPECT_THAT(future.await(), is(equal_to(5)));
}

TEST_CASE(TaskletFuture, AndThenVoid) {
    tasklet::Scheduler scheduler(4, 64, false);
    scheduler.run([] {
        Atomic<size_t> counter;
        auto future = tasklet::schedule([&] {
            counter.fetch_add(1);
        }).and_then([&] {
            EXPECT_THAT(counter.fetch_add(1), is(equal_to(1)));
        });
        future.await();
        EXPECT_THAT(counter.load(), is(equal_to(2)));
    });
}

static int mult(int value) {
    return value * 2;
}

TEST_CASE(TaskletFuture, AndThenTrivial) {
    tasklet::Scheduler scheduler(4, 64, false);
    scheduler.run([] {
        // clang-format off
        auto future = tasklet::schedule([] {
            return 5;
        }).and_then([](int value) {
            return value + 1;
        }).and_then(&mult);
        // clang-format on
        EXPECT_THAT(future.await(), is(equal_to(12)));
    });
}

TEST_CASE(TaskletFuture, AndThenToVoid) {
    tasklet::Scheduler scheduler(4, 64, false);
    scheduler.run([] {
        tasklet::schedule([] {
            return 10;
            // clang-format off
        }).and_then([](int value) {
            EXPECT_THAT(value, is(equal_to(10)));
            // clang-format on
        }).await();
    });
}

TEST_CASE(TaskletFuture, AndThenToOther) {
    tasklet::Scheduler scheduler(4, 64, false);
    scheduler.run([] {
        tasklet::schedule([] {
            return 10;
            // clang-format off
        }).and_then([](int value) {
            return value > 0;
        }).and_then([](bool value) {
            EXPECT_TRUE(value);
            // clang-format on
        }).await();
    });
}

TEST_CASE(TaskletFuture, AndThenMove) {
    tasklet::Scheduler scheduler(4, 64, false);
    scheduler.run([] {
        size_t destruct_count = 0;
        tasklet::schedule([&] {
            return test::MoveTester(destruct_count);
            // clang-format off
        }).and_then([](test::MoveTester &&tester) {
            // clang-format on
            EXPECT_FALSE(tester.is_empty());
        }).await();
        EXPECT_THAT(destruct_count, is(equal_to(1)));
    });
}

TEST_CASE(TaskletFuture, Empty) {
    tasklet::Future<void> future;
    EXPECT_FALSE(future.is_valid());
}

TEST_CASE(TaskletFuture, SwapEmpty) {
    auto promise = vull::adopt_shared(new tasklet::SharedPromise<void>);
    tasklet::Future<void> foo(SharedPtr<tasklet::SharedPromise<void>>{promise});
    tasklet::Future<void> bar;
    EXPECT_TRUE(foo.is_valid());
    vull::swap(foo, bar);
    EXPECT_FALSE(foo.is_valid());
    EXPECT_TRUE(bar.is_valid());
}

TEST_CASE(TaskletFuture, IsComplete) {
    auto promise = vull::adopt_shared(new tasklet::SharedPromise<void>);
    tasklet::Future<void> future(SharedPtr<tasklet::SharedPromise<void>>{promise});
    ASSERT_TRUE(future.is_valid());
    EXPECT_FALSE(future.is_complete());
    promise->fulfill();
    EXPECT_TRUE(future.is_complete());
}
