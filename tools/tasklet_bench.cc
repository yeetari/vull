#include <vull/container/vector.hh>
#include <vull/core/log.hh>
#include <vull/maths/random.hh>
#include <vull/platform/timer.hh>
#include <vull/support/args_parser.hh>
#include <vull/support/atomic.hh>
#include <vull/support/scoped_lock.hh>
#include <vull/support/string.hh>
#include <vull/support/utility.hh>
#include <vull/tasklet/functions.hh>
#include <vull/tasklet/future.hh>
#include <vull/tasklet/io.hh>
#include <vull/tasklet/latch.hh>
#include <vull/tasklet/mutex.hh>
#include <vull/tasklet/scheduler.hh>

#include <stdint.h>
#include <stdlib.h>

using namespace vull;

namespace {

void blocking_io_dispatch(uint32_t count) {
    tasklet::schedule([count] {
        for (uint32_t i = 0; i < count; i++) {
            tasklet::submit_io_request<tasklet::NopRequest>().await();
        }
    }).await();
}

void parallel_io_dispatch(uint32_t count) {
    tasklet::schedule([count] {
        Vector<tasklet::Future<tasklet::IoResult>> futures;
        futures.ensure_capacity(count);
        for (uint32_t i = 0; i < count; i++) {
            futures.push(tasklet::submit_io_request<tasklet::NopRequest>());
        }
        // TODO: Not having a Future::wait_all() will skew this benchmark a bit.
        for (auto &future : futures) {
            future.await();
        }
    }).await();
}

double estimate_pi(uint32_t tasklet_count) {
    Vector<tasklet::Future<size_t>> futures;
    futures.ensure_capacity(tasklet_count);
    for (size_t i = 0; i < tasklet_count; i++) {
        futures.push(tasklet::schedule([] {
            size_t count = 0;
            for (size_t i = 0; i < 1'000'000; i++) {
                float x = vull::linear_rand(0.0f, 1.0f);
                float y = vull::linear_rand(0.0f, 1.0f);
                if (x * x + y * y <= 1.0f) {
                    count++;
                }
            }
            return count;
        }));
    }

    size_t total_count = 0;
    for (auto &future : futures) {
        total_count += future.await();
    }
    return 4.0 * static_cast<double>(total_count) / (tasklet_count * 1'000'000);
}

void ping_pong(size_t count) {
    tasklet::Mutex mutex;
    bool ping_turn = true;
    auto ping = tasklet::schedule([&] {
        for (size_t i = 0; i < count; i++) {
            while (true) {
                tasklet::yield();
                ScopedLock lock(mutex);
                if (ping_turn) {
                    ping_turn = false;
                    break;
                }
            }
        }
    });
    auto pong = tasklet::schedule([&] {
        for (size_t i = 0; i < count; i++) {
            while (true) {
                tasklet::yield();
                ScopedLock lock(mutex);
                if (!ping_turn) {
                    ping_turn = true;
                    break;
                }
            }
        }
    });
    ping.await();
    pong.await();
}

bool is_prime(size_t n) {
    if (n == 2) {
        return true;
    }
    if (n < 2 || n % 2 == 0) {
        return false;
    }
    auto root = static_cast<size_t>(__builtin_sqrt(static_cast<double>(n)));
    for (size_t i = 3; i <= root; i += 2) {
        if (n % i == 0) {
            return false;
        }
    }
    return true;
}

void find_primes(Vector<size_t> &primes, tasklet::Mutex &mutex, size_t start, size_t end) {
    Vector<size_t> local_primes;
    for (size_t i = start; i < end; i++) {
        if (is_prime(i)) {
            local_primes.push(i);
        }
    }

    ScopedLock lock(mutex);
    primes.extend(local_primes);
}

uint32_t find_primes(uint32_t tasklet_count) {
    Vector<size_t> primes;
    tasklet::Mutex mutex;
    tasklet::Latch latch(tasklet_count);
    for (size_t i = 0; i < tasklet_count; i++) {
        tasklet::schedule([&, i] {
            size_t start = i * 100'000 + 2;
            size_t end = start + 100'000;
            find_primes(primes, mutex, start, end);
            latch.count_down(1);
        });
    }
    latch.wait();
    return primes.size();
}

void do_stress_test(uint32_t tasklet_count) {
    vull::info("[stress] Spawning {} tasklets", tasklet_count);
    Atomic<uint32_t> atomic_counter;
    uint32_t shared_counter = 0;
    tasklet::Latch latch(tasklet_count);
    tasklet::Mutex mutex;
    for (uint32_t i = 0; i < tasklet_count; i++) {
        tasklet::schedule([&] {
            auto io_future = tasklet::submit_io_request<tasklet::NopRequest>();
            for (uint32_t j = 0; j < 5; j++) {
                tasklet::yield();
            }

            mutex.lock();
            shared_counter++;
            mutex.unlock();

            tasklet::schedule([io_future] {
                tasklet::yield();
                io_future.await();
            }).and_then([&] {
                tasklet::yield();
                atomic_counter.fetch_add(1, vull::memory_order_relaxed);
                latch.count_down();
            });
        });
    }
    latch.wait();
    vull::info("[stress] Counters: {} {}", atomic_counter.load(vull::memory_order_relaxed), shared_counter);
}

} // namespace

int main(int argc, char **argv) {
    bool stress_test = false;
    uint32_t thread_count = 0;

    ArgsParser args_parser("tasklet-bench", "Tasklet Benchmarks", "0.1.0");
    args_parser.add_flag(stress_test, "Run stress test", "stress");
    args_parser.add_option(thread_count, "Tasklet worker thread count", "threads");
    if (auto result = args_parser.parse_args(argc, argv); result != ArgsParseResult::Continue) {
        return result == ArgsParseResult::ExitSuccess ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    vull::open_log();
    vull::set_log_colours_enabled(true);

    tasklet::Scheduler scheduler(thread_count);
    scheduler.run([&] {
        // Warmup scheduler.
        tasklet::Latch latch(512);
        for (size_t i = 0; i < 512; i++) {
            tasklet::schedule([&] {
                latch.count_down(1);
            });
        }
        latch.wait();

        if (stress_test) {
            do_stress_test(100'000 * scheduler.thread_count());
            return;
        }

        // Blocking IO dispatch benchmark.
        {
            constexpr size_t count = 8192;
            platform::Timer timer;
            blocking_io_dispatch(count);
            vull::info("[bench] Completed {} blocking IO dispatches in {} ms", count, timer.elapsed() * 1000.0f);
        }

        // Parallel IO dispatch benchmark.
        {
            constexpr size_t count = 16384;
            platform::Timer timer;
            parallel_io_dispatch(count);
            vull::info("[bench] Completed {} parallel IO dispatches in {} ms", count, timer.elapsed() * 1000.0f);
        }

        // Monte Carlo pi estimation.
        {
            platform::Timer timer;
            double pi_estimate = estimate_pi(512);
            vull::info("[bench] Estimated pi={} in {} ms", pi_estimate, timer.elapsed() * 1000.0f);
        }

        // Ping pong mutex test.
        {
            platform::Timer timer;
            ping_pong(100'000);
            vull::info("[bench] Mutex ping pong completed in {} ms", timer.elapsed() * 1000.0f);
        }

        // Prime search.
        {
            platform::Timer timer;
            uint32_t prime_count = find_primes(256);
            vull::info("[bench] Found {} primes in {} ms", prime_count, timer.elapsed() * 1000.0f);
        }
    });
}
