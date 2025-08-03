#include <vull/container/vector.hh>
#include <vull/platform/thread.hh>
#include <vull/support/atomic.hh>
#include <vull/support/result.hh>
#include <vull/support/utility.hh>
#include <vull/tasklet/functions.hh>
#include <vull/tasklet/promise.hh>
#include <vull/tasklet/scheduler.hh>
#include <vull/tasklet/tasklet.hh>
#include <vull/test/assertions.hh>
#include <vull/test/matchers.hh>
#include <vull/test/test.hh>

#include <stdint.h>

using namespace vull;
using namespace vull::test::matchers;

TEST_CASE(TaskletPromise, TaskletWakeTasklet) {
    tasklet::Scheduler scheduler(1, 64, false);
    scheduler.run([] {
        tasklet::Promise<int> promise;
        tasklet::schedule([&] {
            promise.fulfill(5);
        });
        promise.wait();
        EXPECT_THAT(promise.value(), is(equal_to(5)));
    });
}

TEST_CASE(TaskletPromise, ThreadWakeThread) {
    tasklet::Promise<int> promise;
    auto thread = VULL_EXPECT(platform::Thread::create([&] {
        promise.fulfill(5);
    }));
    promise.wait();
    EXPECT_THAT(promise.value(), is(equal_to(5)));
}

TEST_CASE(TaskletPromise, TaskletWakeThread) {
    tasklet::Scheduler scheduler(1, 64, false);
    scheduler.run([] {
        Atomic<bool> ready;
        tasklet::Promise<int> promise;
        auto thread = VULL_EXPECT(platform::Thread::create([&] {
            ready.store(true);
            promise.wait();
            EXPECT_THAT(promise.value(), is(equal_to(5)));
        }));
        while (!ready.load()) {
        }
        promise.fulfill(5);
    });
}

TEST_CASE(TaskletPromise, ThreadWakeTasklet) {
    tasklet::Scheduler scheduler(4, 64, false);
    scheduler.run([&] {
        tasklet::Promise<int> promise;
        auto thread = VULL_EXPECT(platform::Thread::create([&] {
            scheduler.setup_thread();
            promise.fulfill(5);
        }));
        promise.wait();
        EXPECT_THAT(promise.value(), is(equal_to(5)));
    });
}

TEST_CASE(TaskletPromise, TaskletWakeMany) {
    tasklet::Scheduler scheduler(4, 64, false);
    scheduler.run([&] {
        tasklet::Promise<int> promise;

        Vector<tasklet::SharedPromise<void> *> tasklets;
        Vector<platform::Thread> threads;
        for (uint32_t i = 0; i < 16; i++) {
            auto *tasklet = new tasklet::PromisedTasklet([&] {
                EXPECT_TRUE(promise.is_fulfilled());
                EXPECT_THAT(promise.value(), is(equal_to(5)));
            });
            tasklet->add_ref();
            tasklets.push(tasklet);
            EXPECT_TRUE(promise.add_waiter(tasklet));

            threads.push(VULL_EXPECT(platform::Thread::create([&] {
                promise.wait();
                EXPECT_THAT(promise.value(), is(equal_to(5)));
            })));
        }

        promise.fulfill(5);
        for (auto *tasklet : tasklets) {
            tasklet->wait();
            tasklet->sub_ref();
        }
        // Threads automatically joined here.
    });
}

TEST_CASE(TaskletPromise, ThreadWakeMany) {
    tasklet::Scheduler scheduler(4, 64, false);
    scheduler.run([&] {
        tasklet::Promise<int> promise;

        Vector<tasklet::SharedPromise<void> *> tasklets;
        Vector<platform::Thread> threads;
        for (uint32_t i = 0; i < 16; i++) {
            auto *tasklet = new tasklet::PromisedTasklet([&] {
                EXPECT_TRUE(promise.is_fulfilled());
                EXPECT_THAT(promise.value(), is(equal_to(5)));
            });
            tasklet->add_ref();
            tasklets.push(tasklet);
            EXPECT_TRUE(promise.add_waiter(tasklet));

            threads.push(VULL_EXPECT(platform::Thread::create([&] {
                promise.wait();
                EXPECT_THAT(promise.value(), is(equal_to(5)));
            })));
        }

        VULL_EXPECT(platform::Thread::create([&] {
            scheduler.setup_thread();
            promise.fulfill(5);
            for (auto *tasklet : tasklets) {
                tasklet->wait();
                tasklet->sub_ref();
            }
        }));
        // Threads automatically joined here.
    });
}

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
