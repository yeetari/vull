#include <vull/container/mpmc_queue.hh>

#include <vull/container/vector.hh>
#include <vull/core/log.hh>
#include <vull/platform/thread.hh>
#include <vull/platform/timer.hh>
#include <vull/support/atomic.hh>
#include <vull/support/result.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>

#include <stddef.h>
#include <stdint.h>

using namespace vull;

namespace {

constexpr size_t k_item_count = 1'000'000;

} // namespace

int main() {
    Atomic<bool> ready;
    Vector<platform::Thread> threads;
    auto queue = vull::make_unique<MpmcQueue<size_t, 20>>();
    for (size_t i = 0; i < 4; i++) {
        threads.push(VULL_EXPECT(platform::Thread::create([&ready, &queue] {
            while (!ready.load()) {
            }

            for (size_t i = 0; i < k_item_count; i++) {
                queue->dequeue([] {
                    platform::Thread::yield();
                });
            }
        })));

        threads.push(VULL_EXPECT(platform::Thread::create([&ready, &queue] {
            while (!ready.load()) {
            }

            for (size_t i = 0; i < k_item_count; i++) {
                queue->enqueue(i, [] {});
            }
        })));
    }

    for (uint32_t i = 0; i < threads.size(); i++) {
        VULL_EXPECT(threads[i].pin_to_core(i));
    }

    platform::Timer timer1;
    ready.store(true);
    for (auto &thread : threads) {
        VULL_EXPECT(thread.join());
    }
    vull::println("Blocking functions took {} ms", timer1.elapsed() * 1000.0f);
    ready.store(false);
    threads.clear();

    for (size_t i = 0; i < 4; i++) {
        threads.push(VULL_EXPECT(platform::Thread::create([&ready, &queue] {
            while (!ready.load()) {
            }

            for (size_t i = 0; i < k_item_count; i++) {
                while (!queue->try_dequeue()) {
                }
            }
        })));

        threads.push(VULL_EXPECT(platform::Thread::create([&ready, &queue] {
            while (!ready.load()) {
            }

            for (size_t i = 0; i < k_item_count; i++) {
                while (!queue->try_enqueue(i)) {
                }
            }
        })));
    }

    for (uint32_t i = 0; i < threads.size(); i++) {
        VULL_EXPECT(threads[i].pin_to_core(i));
    }

    platform::Timer timer2;
    ready.store(true);
    for (auto &thread : threads) {
        VULL_EXPECT(thread.join());
    }
    vull::println("Try functions took {} ms", timer2.elapsed() * 1000.0f);
}
