#pragma once

#include <vull/container/array.hh>
#include <vull/maths/common.hh>
#include <vull/support/tuple.hh>

#include <stdint.h>

namespace vull::vk {

struct MemoryBlock {
    MemoryBlock *prev_free;
    MemoryBlock *next_free;
    MemoryBlock *prev_phys;
    MemoryBlock *next_phys;
    uint32_t offset;
    uint32_t size;
    bool is_free;
};

/**
 * @brief An implementation of the TLSF algorithm for managing external memory, such as Vulkan device memory blocks.
 *
 * Each MemoryPool manages a fixed-size region specified at pool creation time and implements the TLSF algorithm to
 * subdivide the region into smaller blocks. TLSF works using a two-tiered hierarchy of block size classes. The first
 * level is spread across power of twos. Each first level is then made up of multiple second levels, which further
 * divide the space into linearly sized block sizes. Bitsets are used to track which size classes (a first and second
 * level index pair) have any available free blocks. This makes allocation and freeing O(1) time complexity.
 *
 * The main complexity in allocation is handling alignment requirements. A chosen block must be able to handle the
 * worst-case misalignment. The resulting padding from any alignment must then be split into its own block so as to
 * mitigate internal fragmentation. Lots of alignment can still cause external fragmentation, but this is hopefully
 * mitigated by allocating most allocations which have high alignment, such as optimal image layout render targets, into
 * their own dedicated allocations, bypassing this TLSF pool.
 *
 * Freeing a block consists of coalescing neighboring free blocks before returning the block to the free list for its
 * computed size class.
 *
 * Each block is part of a circular physical linked list, which contains all blocks in address order. When a block is
 * free, it is also part of the free list for its given size class.
 *
 * Since this is an external allocator, meaning not managing host-side RAM where a block header can be placed before the
 * real allocated bytes, the block metadata (MemoryBlock objects) need to be managed separately. This is currently done
 * with plain new and delete but a simple free-list could be added in the future.
 */
class MemoryPool {
    using Bitset = uint32_t;

    /**
     * @brief The minimum allocation size in bytes. This effectively sets the minimum alignment.
     */
    static constexpr uint32_t k_minimum_allocation_size = 256;

    /**
     * @brief The number of exponential first level size classes in the pool.
     *
     * We set it to the maximum available given the size type (32 bits), minus the bits that would be needed for the
     * minimum allocation size, since those would be unused. This allows the pool to manage 4 GiB total.
     */
    static constexpr uint32_t k_fl_count = sizeof(Bitset) * 8 - vull::log2(k_minimum_allocation_size);

    /**
     * @brief The number of linear second levels per first level size class.
     *
     * We set it to the maximum available given our bitset size (32 bits). It could be lower but should always be a
     * power of two.
     */
    static constexpr uint32_t k_sl_count = sizeof(Bitset) * 8;

    const uint32_t m_total_size;
    uint32_t m_used_size{};
    Bitset m_fl_bitset{};
    Array<Bitset, k_fl_count> m_sl_bitsets{};
    Array<Array<MemoryBlock *, k_sl_count>, k_fl_count> m_free_map{};
    MemoryBlock *m_root_block{nullptr};

    /**
     * @brief Computes the optimal two-level size class of the given size.
     *
     * @param size the size in bytes
     * @return a tuple of the first and second level indices
     */
    static Tuple<uint32_t, uint32_t> size_mapping(uint32_t size);

    /**
     * @brief Links the given free block into the free list of its size class.
     */
    void link_block(MemoryBlock *block);

    /**
     * @brief Unlinks the given free block from the free list of its size class and marks it as non-free.
     */
    void unlink_block(MemoryBlock *block, uint32_t fl_index, uint32_t sl_index);

public:
    explicit MemoryPool(uint32_t total_size);
    MemoryPool(const MemoryPool &) = delete;
    MemoryPool(MemoryPool &&) = delete;
    ~MemoryPool();

    MemoryPool &operator=(const MemoryPool &) = delete;
    MemoryPool &operator=(MemoryPool &&) = delete;

    /**
     * @brief Attempts to allocate a block from the pool with the given size and offset alignment.
     *
     * @return a MemoryBlock on success; nullptr if the pool could not accommodate the request
     */
    MemoryBlock *allocate(uint32_t size, uint32_t alignment);

    /**
     * @brief Returns the given block back to the pool. The block should no longer be used after this.
     *
     * @param block the block to free
     */
    void free(MemoryBlock *block);

    /**
     * @brief Finds the size of the largest available free block in the pool.
     */
    uint32_t largest_free_block_size() const;

    /**
     * @brief Validates the internal structure of the pool.
     *
     * @return true if the validation was successful; false otherwise
     */
    bool validate() const;

    /**
     * @brief Returns the total number of bytes managed by the pool.
     */
    uint32_t total_size() const { return m_total_size; }

    /**
     * @brief Returns the total amount of used space of the pool in bytes.
     */
    uint32_t used_size() const { return m_used_size; }
};

} // namespace vull::vk
