#include <vull/container/array.hh>
#include <vull/core/log.hh>
#include <vull/maths/random.hh>
#include <vull/platform/timer.hh>
#include <vull/support/args_parser.hh>
#include <vull/support/string.hh>
#include <vull/support/utility.hh>
#include <vull/vulkan/memory.hh>

#include <stdint.h>
#include <stdlib.h>

using namespace vull;

static uint32_t next_pot(uint32_t v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

static int do_stress_test() {
    Array<vk::MemoryBlock *, 256> blocks{};
    vk::MemoryPool pool(1073741824u);
    size_t counter = 0;
    while (true) {
        uint32_t index = vull::linear_rand(0u, blocks.size() - 1);
        if (blocks[index] != nullptr) {
            pool.free(vull::exchange(blocks[index], nullptr));
        } else {
            uint32_t size = 0;
            if (vull::linear_rand(0u, 100u) < 10) {
                size = vull::linear_rand(1024u * 1024 * 4, 1024u * 1024 * 64);
            } else {
                size = vull::linear_rand(1u, 1024u * 1024 * 4);
            }
            blocks[index] = pool.allocate(size, next_pot(vull::linear_rand(1u, 16384u)));
        }
        counter++;
        if (counter % 10000 == 0) {
            vull::info("[stress] Reached {} allocs/frees - {} bytes used", counter, pool.used_size());
            if (!pool.validate()) {
                return EXIT_FAILURE;
            }
        }
    }
}

int main(int argc, char **argv) {
    bool stress_test = false;
    ArgsParser args_parser("tlsf-bench", "TLSF Allocator Benchmarks", "0.1.0");
    args_parser.add_flag(stress_test, "Run stress test", "stress", 's');
    if (auto result = args_parser.parse_args(argc, argv); result != ArgsParseResult::Continue) {
        return result == ArgsParseResult::ExitSuccess ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    vull::open_log();
    vull::set_log_colours_enabled(true);

    if (stress_test) {
        return do_stress_test();
    }

    constexpr size_t total_alloc_count = 10'000'000;
    Array<vk::MemoryBlock *, 64> blocks{};
    vk::MemoryPool pool(1073741824u);
    size_t alloc_count = 0;
    size_t free_count = 0;
    platform::Timer timer;
    while (free_count < total_alloc_count) {
        uint32_t index = vull::linear_rand(0u, blocks.size() - 1);
        if (alloc_count == total_alloc_count || vull::linear_rand(0u, 100u) >= 50) {
            if (blocks[index] != nullptr) {
                pool.free(vull::exchange(blocks[index], nullptr));
                free_count++;
            }
        } else {
            if (blocks[index] == nullptr) {
                blocks[index] = pool.allocate(vull::linear_rand(1u, 1024u * 1024), 1);
                alloc_count++;
            }
        }
    }

    const float elapsed = timer.elapsed();
    vull::info("[bench] Completed {} allocations in {} ms", total_alloc_count, elapsed * 1000.0f);
    vull::info("[bench] Allocs+frees per second: {}", static_cast<size_t>(total_alloc_count / elapsed));
}
