#include <vull/container/work_stealing_queue.hh>

#include <vull/container/vector.hh>
#include <vull/support/algorithm.hh>
#include <vull/support/atomic.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>
#include <vull/test/assertions.hh>
#include <vull/test/matchers.hh>
#include <vull/test/test.hh>

#include <pthread.h>
#include <stdlib.h>
#include <time.h>

using namespace vull;
using namespace vull::test::matchers;

TEST_CASE(WorkStealingQueue, Empty) {
    auto wsq = vull::make_unique<WorkStealingQueue<unsigned>>();
    EXPECT_THAT(*wsq, is(empty()));
    EXPECT_THAT(wsq->size(), is(equal_to(0)));
    EXPECT_THAT(wsq->dequeue(), is(null()));
    EXPECT_THAT(wsq->steal(), is(null()));
}

TEST_CASE(WorkStealingQueue, Enqueue) {
    auto wsq = vull::make_unique<WorkStealingQueue<unsigned>>();
    for (unsigned i = 0; i < 512; i++) {
        EXPECT_TRUE(wsq->enqueue(i));
    }
    EXPECT_THAT(*wsq, is(not_(empty())));
    EXPECT_THAT(wsq->size(), is(equal_to(512)));
}

TEST_CASE(WorkStealingQueue, EnqueueDequeue) {
    auto wsq = vull::make_unique<WorkStealingQueue<unsigned>>();
    for (unsigned i = 0; i < 512; i++) {
        EXPECT_TRUE(wsq->enqueue(i));
    }
    for (unsigned i = 0; i < 512; i++) {
        EXPECT_THAT(wsq->dequeue(), is(equal_to(512 - i - 1)));
    }
    EXPECT_THAT(*wsq, is(empty()));
    EXPECT_THAT(wsq->dequeue(), is(null()));
    EXPECT_THAT(wsq->steal(), is(null()));
}

TEST_CASE(WorkStealingQueue, EnqueueSteal) {
    auto wsq = vull::make_unique<WorkStealingQueue<unsigned>>();
    for (unsigned i = 0; i < 512; i++) {
        EXPECT_TRUE(wsq->enqueue(i));
    }
    for (unsigned i = 0; i < 512; i++) {
        EXPECT_THAT(wsq->steal(), is(equal_to(i)));
    }
    EXPECT_THAT(*wsq, is(empty()));
    EXPECT_THAT(wsq->dequeue(), is(null()));
    EXPECT_THAT(wsq->steal(), is(null()));
}

TEST_CASE(WorkStealingQueue, OverCapacity) {
    auto wsq = vull::make_unique<WorkStealingQueue<unsigned, 1>>();
    for (unsigned i = 0; i < 2; i++) {
        EXPECT_TRUE(wsq->enqueue(0u));
    }
    EXPECT_FALSE(wsq->enqueue(0u));
}

TEST_CASE(WorkStealingQueue, Threaded) {
    auto wsq = vull::make_unique<WorkStealingQueue<unsigned>>();
    Vector<pthread_t> consumer_threads(4);
    Vector<Vector<unsigned>> consumer_popped(consumer_threads.size());
    Atomic<uint32_t> popped_count;

    struct ConsumerData {
        WorkStealingQueue<unsigned> &wsq;
        Vector<unsigned> &consumer_popped;
        Atomic<uint32_t> &popped_count;
    };
    Vector<ConsumerData> consumer_data;
    for (uint32_t i = 0; i < consumer_threads.size(); i++) {
        consumer_data.push({
            .wsq = *wsq,
            .consumer_popped = consumer_popped[i],
            .popped_count = popped_count,
        });
    }

    for (uint32_t i = 0; i < consumer_threads.size(); i++) {
        ASSERT_THAT(pthread_create(&consumer_threads[i], nullptr,
                                   +[](void *ptr) {
            auto *data = static_cast<ConsumerData *>(ptr);
            auto seed = static_cast<unsigned>(time(nullptr));
            while (data->popped_count.load() != 1024) {
                if (rand_r(&seed) % 3 == 0) {
                    if (auto elem = data->wsq.steal()) {
                        data->consumer_popped.push(*elem);
                        data->popped_count.fetch_add(1);
                    }
                }
            }
            return static_cast<void *>(nullptr);
        }, &consumer_data[i]),
                    is(equal_to(0)));
    }

    // Main thread will act as the single producer.
    Vector<unsigned> producer_popped;
    auto seed = static_cast<unsigned>(time(nullptr));
    for (unsigned i = 0; i < 1024;) {
        if (int num = rand_r(&seed) % 3; num == 0) {
            EXPECT_TRUE(wsq->enqueue(i++));
        } else if (num == 1) {
            if (auto elem = wsq->dequeue()) {
                producer_popped.push(*elem);
                popped_count.fetch_add(1);
            }
        }
    }

    for (pthread_t consumer_thread : consumer_threads) {
        pthread_join(consumer_thread, nullptr);
    }
    EXPECT_THAT(*wsq, is(empty()));

    Vector<unsigned> all_popped;
    for (const auto &popped : consumer_popped) {
        for (unsigned elem : popped) {
            all_popped.push(elem);
        }
    }
    for (unsigned elem : producer_popped) {
        all_popped.push(elem);
    }
    EXPECT_THAT(all_popped.size(), is(equal_to(1024)));

    vull::sort(all_popped, [](unsigned a, unsigned b) {
        return a > b;
    });
    for (unsigned i = 0; i < all_popped.size(); i++) {
        EXPECT_THAT(all_popped[i], is(equal_to(i)));
    }
}
