#pragma once

#include <vull/container/array.hh>
#include <vull/container/vector.hh>
#include <vull/maths/common.hh>
#include <vull/support/flag_bitset.hh>
#include <vull/support/optional.hh>
#include <vull/support/tuple.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/tasklet/mutex.hh>
#include <vull/vulkan/vulkan.hh>

#include <stdint.h>

namespace vull::vk {

class Context;

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

class DeviceMemoryHeap;
class DeviceMemoryPool;

class DeviceMemoryAllocation {
    DeviceMemoryHeap *m_heap{nullptr};
    vkb::DeviceMemory m_device_memory{nullptr};
    DeviceMemoryPool *m_pool{nullptr};
    MemoryBlock *m_block{nullptr};
    void *m_mapped_data{nullptr};

public:
    DeviceMemoryAllocation() = default;
    DeviceMemoryAllocation(DeviceMemoryHeap *heap, vkb::DeviceMemory device_memory, DeviceMemoryPool *pool,
                           MemoryBlock *block, void *mapped_data)
        : m_heap(heap), m_device_memory(device_memory), m_pool(pool), m_block(block), m_mapped_data(mapped_data) {}
    DeviceMemoryAllocation(DeviceMemoryHeap *heap, vkb::DeviceMemory device_memory, void *mapped_data)
        : m_heap(heap), m_device_memory(device_memory), m_mapped_data(mapped_data) {}
    DeviceMemoryAllocation(const DeviceMemoryAllocation &) = delete;
    DeviceMemoryAllocation(DeviceMemoryAllocation &&);
    ~DeviceMemoryAllocation();

    DeviceMemoryAllocation &operator=(const DeviceMemoryAllocation &) = delete;
    DeviceMemoryAllocation &operator=(DeviceMemoryAllocation &&);

    vkb::Result bind_to(vkb::Buffer buffer) const;
    vkb::Result bind_to(vkb::Image image) const;
    void swap(DeviceMemoryAllocation &other);

    DeviceMemoryHeap &heap() const { return *m_heap; }
    vkb::DeviceMemory device_memory() const { return m_device_memory; }
    DeviceMemoryPool *pool() const { return m_pool; }
    MemoryBlock *block() const { return m_block; }
    void *mapped_data() const { return m_mapped_data; }
    bool is_dedicated() const { return m_block == nullptr; }
};

class DeviceMemoryPool {
    const Context &m_context;
    vkb::DeviceMemory m_memory;
    void *m_mapped_data{nullptr};
    MemoryPool m_pool;

public:
    DeviceMemoryPool(const Context &context, vkb::DeviceMemory memory, vkb::DeviceSize size, void *mapped_data);
    DeviceMemoryPool(const DeviceMemoryPool &) = delete;
    DeviceMemoryPool(DeviceMemoryPool &&) = delete;
    ~DeviceMemoryPool();

    DeviceMemoryPool &operator=(const DeviceMemoryPool &) = delete;
    DeviceMemoryPool &operator=(DeviceMemoryPool &&) = delete;

    Tuple<MemoryBlock *, void *> allocate(vkb::DeviceSize size, vkb::DeviceSize alignment);
    void free(MemoryBlock *block);
    bool is_empty() const;

    const Context &context() const { return m_context; }
    vkb::DeviceMemory memory() const { return m_memory; }
    void *mapped_data() const { return m_mapped_data; }
    vkb::DeviceSize size() const { return static_cast<vkb::DeviceSize>(m_pool.total_size()); }
};

class DeviceMemoryHeap {
    Context &m_context;
    const uint32_t m_memory_type_index;
    const vkb::DeviceSize m_pool_size;
    const bool m_is_mappable{false};

    Vector<UniquePtr<DeviceMemoryPool>> m_pools;
    tasklet::Mutex m_pools_mutex;

    uint32_t find_pool_index(DeviceMemoryPool *pool) const;

public:
    DeviceMemoryHeap(Context &context, uint32_t memory_type_index, vkb::DeviceSize pool_size, bool is_mappable)
        : m_context(context), m_memory_type_index(memory_type_index), m_pool_size(pool_size),
          m_is_mappable(is_mappable) {}

    vkb::Result allocate_device_memory(vkb::DeviceSize size, vkb::Buffer dedicated_buffer, vkb::Image dedicated_image,
                                       float priority, vkb::DeviceMemory *memory, void **mapped_data);
    Optional<DeviceMemoryAllocation> allocate(vkb::DeviceSize size, vkb::DeviceSize alignment,
                                              vkb::Buffer dedicated_buffer, vkb::Image dedicated_image,
                                              float dedicated_priority);
    void free(const DeviceMemoryAllocation &allocation);

    Context &context() const { return m_context; }
    uint32_t memory_type_index() const { return m_memory_type_index; }
};

enum class DeviceMemoryFlag : uint32_t {
    None = 0,
    HostRandomAccess,
    HostSequentialWrite,
    Staging,

    /**
     * @brief Specifies that this allocation should be allocated in its own VkDeviceMemory block.
     *
     * This flag is automatically added if the driver PREFERS/REQUIRES a dedicated allocation.
     */
    PreferDedicated,

    /**
     * @brief Specifies to the Vulkan driver that this allocation should be high priority, which may give it precedence
     * during an overcommit out-of-memory situation.
     *
     * This flag only has effect if the allocation ends up being dedicated.
     */
    HighPriority,
};

using DeviceMemoryFlags = FlagBitset<DeviceMemoryFlag>;

class DeviceMemoryAllocator {
    Context &m_context;
    vkb::PhysicalDeviceMemoryProperties m_memory_properties{};
    vkb::DeviceSize m_buffer_image_granularity{};
    vkb::DeviceSize m_max_memory_allocation_size{};
    uint32_t m_max_memory_allocation_count{};

    // TODO: This always has a fixed size so doesn't necessarily need to be unique ptrs.
    Vector<UniquePtr<DeviceMemoryHeap>> m_heaps;

public:
    explicit DeviceMemoryAllocator(Context &context);

    Optional<uint32_t> find_best_type_index(DeviceMemoryFlags flags, uint32_t memory_type_bits) const;
    Optional<DeviceMemoryAllocation> allocate_memory(vkb::DeviceSize size, vkb::DeviceSize alignment,
                                                     DeviceMemoryFlags flags, uint32_t memory_type_bits,
                                                     vkb::Buffer buffer, vkb::Image image);
    Optional<DeviceMemoryAllocation> allocate_for(vkb::Buffer buffer, DeviceMemoryFlags flags);
    Optional<DeviceMemoryAllocation> allocate_for(vkb::Image image, DeviceMemoryFlags flags);
};

} // namespace vull::vk
