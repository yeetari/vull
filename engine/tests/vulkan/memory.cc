#include <vull/test/assertions.hh>
#include <vull/test/matchers.hh>
#include <vull/test/test.hh>
#include <vull/vulkan/memory.hh>

using namespace vull;
using namespace vull::test::matchers;

TEST_CASE(VulkanMemoryPool, Empty) {
    vk::MemoryPool pool(1024);
    EXPECT_THAT(pool.largest_free_block_size(), is(equal_to(1024)));
    EXPECT_THAT(pool.total_size(), is(equal_to(1024)));
    EXPECT_THAT(pool.used_size(), is(equal_to(0)));
    EXPECT_TRUE(pool.validate());
}

TEST_CASE(VulkanMemoryPool, SingleMaxAllocation) {
    vk::MemoryPool pool(1024);
    auto *block = pool.allocate(1024, 1);
    ASSERT_THAT(block, is(not_(null())));
    EXPECT_THAT(block->offset, is(equal_to(0)));
    EXPECT_THAT(block->size, is(equal_to(1024)));
    EXPECT_THAT(pool.used_size(), is(equal_to(1024)));
    EXPECT_TRUE(pool.validate());
    pool.free(block);
    EXPECT_THAT(pool.used_size(), is(equal_to(0)));
    EXPECT_TRUE(pool.validate());
}

TEST_CASE(VulkanMemoryPool, FailedAllocation) {
    vk::MemoryPool pool(1024);
    auto *block = pool.allocate(2048, 1);
    EXPECT_THAT(block, is(null()));
    EXPECT_THAT(pool.used_size(), is(equal_to(0)));
    EXPECT_TRUE(pool.validate());
}

TEST_CASE(VulkanMemoryPool, Alignment) {
    vk::MemoryPool pool(32768);
    auto *first = pool.allocate(1024, 1);
    auto *second = pool.allocate(1024, 16384);
    ASSERT_THAT(first, is(not_(null())));
    ASSERT_THAT(second, is(not_(null())));
    EXPECT_THAT(second->offset, is(equal_to(16384)));
    EXPECT_TRUE(pool.validate());
    pool.free(second);
    pool.free(first);
    EXPECT_TRUE(pool.validate());
}

TEST_CASE(VulkanMemoryPool, AlignmentExhaustion) {
    vk::MemoryPool pool(32768);
    auto *first = pool.allocate(1, 16384);
    auto *second = pool.allocate(1, 16384);
    auto *third = pool.allocate(1, 16384);
    ASSERT_THAT(first, is(not_(null())));
    ASSERT_THAT(second, is(not_(null())));
    EXPECT_THAT(third, is(null()));

    EXPECT_TRUE(pool.validate());
    EXPECT_THAT(first->offset, is(equal_to(0)));
    EXPECT_THAT(second->offset, is(equal_to(16384)));

    pool.free(first);
    pool.free(second);
    EXPECT_TRUE(pool.validate());
    EXPECT_THAT(pool.used_size(), is(equal_to(0)));
    EXPECT_THAT(pool.largest_free_block_size(), is(equal_to(32768)));
}

TEST_CASE(VulkanMemoryPool, AlignmentExhaustionFalseNegative) {
    // Tests current behaviour of the pool which may change in the future.
    vk::MemoryPool pool(32768);
    auto *first = pool.allocate(16384, 16384);
    auto *second = pool.allocate(16384, 16384);
    ASSERT_THAT(first, is(not_(null())));
    EXPECT_THAT(second, is(null()));
    pool.free(first);
    EXPECT_TRUE(pool.validate());
    EXPECT_THAT(pool.used_size(), is(equal_to(0)));
    EXPECT_THAT(pool.largest_free_block_size(), is(equal_to(32768)));
}
