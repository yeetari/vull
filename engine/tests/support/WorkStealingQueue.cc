#include <vull/support/WorkStealingQueue.hh>

#include <vull/support/Algorithm.hh>
#include <vull/support/Test.hh>
#include <vull/support/UniquePtr.hh>

#include <pthread.h>
#include <stdlib.h>

using namespace vull;

TEST_SUITE(WorkStealingQueue, {
    ;
    TEST_CASE(EnqueueDequeue) {
        auto wsq = vull::make_unique<WorkStealingQueue<unsigned>>();
        EXPECT(wsq->empty());
        for (unsigned i = 0; i < 512; i++) {
            EXPECT(wsq->enqueue(unsigned(i)));
        }
        EXPECT(wsq->size() == 512);
        for (unsigned i = 0; i < 512; i++) {
            auto elem = wsq->dequeue();
            EXPECT(elem);
            EXPECT(*elem == 512 - i - 1);
        }
        EXPECT(wsq->empty());
        EXPECT(!wsq->dequeue());
        EXPECT(!wsq->steal());
    }

    TEST_CASE(EnqueueSteal) {
        auto wsq = vull::make_unique<WorkStealingQueue<unsigned>>();
        for (unsigned i = 0; i < 512; i++) {
            EXPECT(wsq->enqueue(unsigned(i)));
        }
        for (unsigned i = 0; i < 512; i++) {
            auto elem = wsq->steal();
            EXPECT(elem);
            EXPECT(*elem == i);
        }
        EXPECT(wsq->empty());
        EXPECT(!wsq->dequeue());
        EXPECT(!wsq->steal());
    }

    TEST_CASE(OverCapacity) {
        auto wsq = vull::make_unique<WorkStealingQueue<unsigned, 1>>();
        for (unsigned i = 0; i < 2; i++) {
            EXPECT(wsq->enqueue(0u));
        }
        EXPECT(!wsq->enqueue(0u));
    }

    TEST_CASE(Threaded) {
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
            EXPECT(pthread_create(
                       &consumer_threads[i], nullptr,
                       +[](void *ptr) {
                           auto *data = static_cast<ConsumerData *>(ptr);
                           auto seed = static_cast<unsigned>(time(nullptr));
                           while (data->popped_count.load(MemoryOrder::Relaxed) != 1024) {
                               if (rand_r(&seed) % 3 == 0) {
                                   if (auto elem = data->wsq.steal()) {
                                       data->consumer_popped.push(*elem);
                                       data->popped_count.fetch_add(1, MemoryOrder::Relaxed);
                                   }
                               }
                           }
                           return static_cast<void *>(nullptr);
                       },
                       &consumer_data[i]) == 0);
        }

        // Main thread will act as the single producer.
        Vector<unsigned> producer_popped;
        auto seed = static_cast<unsigned>(time(nullptr));
        for (unsigned i = 0; i < 1024;) {
            if (int num = rand_r(&seed) % 3; num == 0) {
                EXPECT(wsq->enqueue(unsigned(i++)));
            } else if (num == 1) {
                if (auto elem = wsq->dequeue()) {
                    producer_popped.push(*elem);
                    popped_count.fetch_add(1, MemoryOrder::Relaxed);
                }
            }
        }

        for (pthread_t consumer_thread : consumer_threads) {
            pthread_join(consumer_thread, nullptr);
        }
        EXPECT(wsq->empty());

        Vector<unsigned> all_popped;
        for (const auto &popped : consumer_popped) {
            for (unsigned elem : popped) {
                all_popped.push(elem);
            }
        }
        for (unsigned elem : producer_popped) {
            all_popped.push(elem);
        }
        EXPECT(all_popped.size() == 1024);

        vull::sort(all_popped, [](unsigned a, unsigned b) {
            return a > b;
        });
        for (unsigned i = 0; i < all_popped.size(); i++) {
            EXPECT(all_popped[i] == i);
        }
    }
})
