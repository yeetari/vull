#include <vull/vulkan/memory.hh>

#include <vull/container/array.hh>
#include <vull/container/vector.hh>
#include <vull/core/log.hh>
#include <vull/core/tracing.hh>
#include <vull/maths/common.hh>
#include <vull/support/algorithm.hh>
#include <vull/support/assert.hh>
#include <vull/support/enum.hh>
#include <vull/support/optional.hh>
#include <vull/support/scoped_lock.hh>
#include <vull/support/string.hh>
#include <vull/support/string_builder.hh>
#include <vull/support/string_view.hh>
#include <vull/support/tuple.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>
#include <vull/tasklet/mutex.hh>
#include <vull/vulkan/context.hh>
#include <vull/vulkan/vulkan.hh>

#include <stdint.h>

namespace vull::vk {

// TODO: Use a free list for the MemoryBlock objects.

MemoryPool::MemoryPool(uint32_t total_size) : m_total_size(total_size) {
    m_root_block = new MemoryBlock{
        .size = total_size,
    };
    m_root_block->prev_phys = m_root_block;
    m_root_block->next_phys = m_root_block;
    link_block(m_root_block);
}

MemoryPool::~MemoryPool() {
    // With all allocations being freed, only the root block should be left.
    VULL_ASSERT(m_used_size == 0, "Not all allocations freed");
    VULL_ASSERT(m_root_block->is_free);
    VULL_ASSERT(m_root_block->prev_phys == m_root_block);
    VULL_ASSERT(m_root_block->next_phys == m_root_block);
    VULL_ASSERT(m_root_block->prev_free == nullptr);
    VULL_ASSERT(m_root_block->next_free == nullptr);
    delete m_root_block;
}

Tuple<uint32_t, uint32_t> MemoryPool::size_mapping(uint32_t size) {
    // Precondition that the size has already been rounded up to the minimum.
    VULL_ASSERT(size >= k_minimum_allocation_size);

    // First level index is simply the power of two size class.
    auto fl_index = vull::log2(size);

    // Normalize the size into the FL range. Mask the top SL index bit so that the resulting index stays in range.
    const auto sl_index = (size >> (fl_index - vull::log2(k_sl_count))) & ~k_sl_count;

    // Offset first level indices so that the first size class corresponds to the minimum allocation size.
    fl_index -= vull::log2(k_minimum_allocation_size);

    return vull::make_tuple(fl_index, sl_index);
}

void MemoryPool::link_block(MemoryBlock *block) {
    VULL_ASSERT(!block->is_free, "Attempting to link already-free block");
    VULL_ASSERT(block->prev_free == nullptr && block->next_free == nullptr);
    block->is_free = true;

    // Insert the block at the head of the free list for the given size class.
    const auto [fl_index, sl_index] = size_mapping(block->size);
    block->next_free = vull::exchange(m_free_map[fl_index][sl_index], block);

    // Insert link to previous head.
    if (block->next_free != nullptr) {
        block->next_free->prev_free = block;
    }

    // Mark the bitsets as containing a free block.
    m_fl_bitset |= 1u << fl_index;
    m_sl_bitsets[fl_index] |= 1u << sl_index;
}

void MemoryPool::unlink_block(MemoryBlock *block, uint32_t fl_index, uint32_t sl_index) {
    VULL_ASSERT(block->is_free);

    // Unlink from free linked list.
    auto *prev_free = block->prev_free;
    auto *next_free = block->next_free;
    if (prev_free != nullptr) {
        prev_free->next_free = next_free;
    }
    if (next_free != nullptr) {
        next_free->prev_free = prev_free;
    }

    // Clear links and free flags.
    block->is_free = false;
    block->prev_free = nullptr;
    block->next_free = nullptr;

    // Check if the block was at the head of its free list, in which case we need to update the head and potentially
    // also clear the corresponding bits in the bitsets.
    if (m_free_map[fl_index][sl_index] == block) {
        // Update the list head.
        m_free_map[fl_index][sl_index] = next_free;

        // If there are no more free blocks for this size class, clear the corresponding bits.
        if (next_free == nullptr) {
            m_sl_bitsets[fl_index] &= ~(1u << sl_index);
            if (m_sl_bitsets[fl_index] == 0) {
                m_fl_bitset &= ~(1u << fl_index);
            }
        }
    }
}

MemoryBlock *MemoryPool::allocate(uint32_t size, uint32_t alignment) {
    // Round up to the minimum allocation size.
    size = vull::max(size, k_minimum_allocation_size);

    // Search for a block that can accomodate the worst-case offset misalignment.
    // TODO: This can result in false negatives if there is a free block whose offset is already aligned well, and we'd
    //       ignore it, i.e. size <= block_size < search_size and offset padding is low. VMA solves this with a best fit
    //       linear search. But this might not be worth implementing considering the pool size vs practical alignment
    //       upper bound.
    auto search_size = size + alignment - 1;

    // Align the search size up to the next size class for if we are in between a linear second level size class.
    search_size = vull::align_up(search_size, 1u << (vull::log2(search_size) - vull::log2(k_sl_count)));

    // Attempt to find a suitable free block by firstly checking the free list for the optimal size class. If that's
    // empty (as indicated by the absence of a set bit in the bitset), we try any available first level size class. If
    // the entire bitset of acceptable sizes is empty, the pool has been exhausted for sizes greater than or equal to
    // 'size'.
    auto [fl_index, sl_index] = size_mapping(search_size);
    auto sl_bitset = m_sl_bitsets[fl_index] & (~0u << sl_index);
    if (sl_bitset == 0) {
        // Second level for our optimal first level size class is empty, consider any available first level.
        const auto fl_bitset = m_fl_bitset & (~0u << (fl_index + 1));
        if (fl_bitset == 0) {
            // The pool is exhausted.
            return nullptr;
        }

        // There is a first level size class than can accomodate our allocation - use it.
        fl_index = vull::ffs(fl_bitset);
        sl_bitset = m_sl_bitsets[fl_index];
    }

    // Recalculate the second level index. If we went up a first level index, then any available second level size class
    // will do. If not we get the first usable index from the masked bitset of acceptable ones.
    sl_index = vull::ffs(sl_bitset);

    // Pull a free block from the list of our chosen size class.
    auto *const block = m_free_map[fl_index][sl_index];
    VULL_ASSERT(block != nullptr);
    VULL_ASSERT(block->is_free, "Attempting allocation of non-free block");
    VULL_ASSERT(block->size >= size, "Attempting allocation with a too small block");

    // Remove the block from its free list.
    unlink_block(block, fl_index, sl_index);

    // Deal with any padding resulting from alignment.
    const auto padding = vull::align_up(block->offset, alignment) - block->offset;
    if (padding > 0) {
        // This should always be the case since we never create blocks misaligned to the minimum size and means we don't
        // have to deal with a separate aligned offset.
        VULL_ASSERT(padding >= k_minimum_allocation_size);

        // The previous block should never be free since otherwise it would be coalesced into our current block.
        VULL_ASSERT(!block->prev_phys->is_free);

        // Create a new block for the padding.
        auto *padding_block = new MemoryBlock{
            .offset = block->offset,
            .size = padding,
        };

        // Insert the padding block into the physical linked list.
        padding_block->next_phys = block;
        padding_block->prev_phys = vull::exchange(block->prev_phys, padding_block);
        padding_block->prev_phys->next_phys = padding_block;

        // Insert the padding block into the free list.
        link_block(padding_block);

        // Fixup the range of our allocation block.
        block->offset += padding;
        block->size -= padding;

        // Make sure that we are now aligned and that our size is still valid.
        VULL_ASSERT(block->offset % alignment == 0);
        VULL_ASSERT(block->size >= size);
    }

    // Check if the block is big enough to split. If so, resize 'block' to be the size of our allocation and create a
    // new block for the remainder of the free space. Align the size up to make sure it's always divisible by the
    // minimum allocation size.
    const auto aligned_size = vull::align_up(size, k_minimum_allocation_size);
    if (block->size - aligned_size >= k_minimum_allocation_size) {
        // Create the remainder block after our block. This keeps our block offset aligned.
        auto *remainder_block = new MemoryBlock{
            .offset = block->offset + aligned_size,
            .size = block->size - aligned_size,
        };
        block->size = aligned_size;

        // Place the remainder block after our block in the physical linked list and place it in the free list.
        remainder_block->prev_phys = block;
        remainder_block->next_phys = vull::exchange(block->next_phys, remainder_block);
        remainder_block->next_phys->prev_phys = remainder_block;
        link_block(remainder_block);
    }

    m_used_size += block->size;
    return block;
}

void MemoryPool::free(MemoryBlock *block) {
    VULL_ASSERT(!block->is_free, "Attempting double-free");
    m_used_size -= block->size;

    // Try to coalesce free neighbouring blocks. The offset check is needed as the physical list is circular.
    if (auto *prev = block->prev_phys; prev->is_free && prev->offset < block->offset) {
        const auto [fl_index, sl_index] = size_mapping(prev->size);
        unlink_block(prev, fl_index, sl_index);

        // Consume 'prev' into 'block'.
        block->offset -= prev->size;
        block->size += prev->size;
        block->prev_phys = prev->prev_phys;
        block->prev_phys->next_phys = block;

        // Update m_root_block if needed.
        if (m_root_block == prev) {
            m_root_block = block;
        }
        delete prev;
    }
    if (auto *next = block->next_phys; next->is_free && next->offset > block->offset) {
        VULL_ASSERT(m_root_block != next);
        const auto [fl_index, sl_index] = size_mapping(next->size);
        unlink_block(next, fl_index, sl_index);

        // Consume 'next' into 'block'.
        block->size += next->size;
        block->next_phys = next->next_phys;
        block->next_phys->prev_phys = block;
        delete next;
    }

    // Insert the block back into its bucket's free list.
    link_block(block);
}

uint32_t MemoryPool::largest_free_block_size() const {
    if (m_fl_bitset == 0) {
        // Pool is completely full.
        return 0;
    }
    const auto fl_index = vull::fls(m_fl_bitset);
    const auto sl_index = vull::fls(m_sl_bitsets[fl_index]);
    return m_free_map[fl_index][sl_index]->size;
}

bool MemoryPool::validate() const {
    if (m_root_block->offset != 0) {
        vull::error("root block not at zero");
        return false;
    }

    // Validate free lists.
    uint32_t free_size = 0;
    for (uint32_t fl_index = 0; fl_index < k_fl_count; fl_index++) {
        const bool fl_empty = (m_fl_bitset & (1u << fl_index)) == 0u;
        for (uint32_t sl_index = 0; sl_index < k_sl_count; sl_index++) {
            const bool sl_empty = (m_sl_bitsets[fl_index] & (1u << sl_index)) == 0u;
            const bool list_empty = m_free_map[fl_index][sl_index] == nullptr;
            if (sl_empty != list_empty || (fl_empty && !sl_empty)) {
                vull::error("class[{}][{}]: flb: {}, slb: {}, list: {}", fl_index, sl_index, fl_empty, sl_empty,
                            list_empty);
                return false;
            }
            if (list_empty) {
                continue;
            }

            MemoryBlock *previous = nullptr;
            for (auto *block = m_free_map[fl_index][sl_index]; block != nullptr; block = block->next_free) {
                if (!block->is_free) {
                    vull::error("block in class[{}][{}] not marked as free", fl_index, sl_index);
                    return false;
                }
                if (block->prev_free != previous) {
                    vull::error("block in class[{}][{}] has bad prev_free", fl_index, sl_index);
                    return false;
                }
                free_size += block->size;
                previous = block;
            }
        }
    }

    if (m_used_size + free_size != m_total_size) {
        vull::error("used_size ({}) + free_size ({}) != total_size ({})", m_used_size, free_size, m_total_size);
        return false;
    }

    // Validate physical list.
    MemoryBlock *previous = nullptr;
    MemoryBlock *block = m_root_block;
    while (true) {
        if (previous != nullptr) {
            if (block->prev_phys != previous) {
                vull::error("block at {h} has bad prev_phys", block->offset);
                return false;
            }
            if (block->size < k_minimum_allocation_size) {
                vull::error("block at {h} has bad size {}", block->offset, block->size);
                return false;
            }
            if (block->offset % k_minimum_allocation_size != 0) {
                vull::error("block at {h} has bad alignment", block->offset);
                return false;
            }
            if (block->offset < previous->offset + previous->size) {
                vull::error("block at [{h}, {h}] overlaps with previous block at [{h}, {h}]", block->offset,
                            block->offset + block->size, previous->offset, previous->offset + previous->size);
                return false;
            }
            if (block->offset != previous->offset + previous->size) {
                const auto gap_size = block->offset - (previous->offset + previous->size);
                vull::error("gap of size {} between blocks [{h}, {h}] and [{h}, {h}]", gap_size, previous->offset,
                            previous->offset + previous->size, block->offset, block->offset + block->size);
                return false;
            }
        }

        previous = block;
        block = block->next_phys;
        if (block == m_root_block) {
            break;
        }
    }

    return true;
}

DeviceMemoryAllocation::DeviceMemoryAllocation(DeviceMemoryAllocation &&other) {
    m_heap = vull::exchange(other.m_heap, nullptr);
    m_device_memory = vull::exchange(other.m_device_memory, nullptr);
    m_pool = vull::exchange(other.m_pool, nullptr);
    m_block = vull::exchange(other.m_block, nullptr);
    m_mapped_data = vull::exchange(other.m_mapped_data, nullptr);
}

DeviceMemoryAllocation::~DeviceMemoryAllocation() {
    if (m_heap != nullptr) {
        m_heap->free(*this);
    }
}

DeviceMemoryAllocation &DeviceMemoryAllocation::operator=(DeviceMemoryAllocation &&other) {
    DeviceMemoryAllocation(vull::move(other)).swap(*this);
    return *this;
}

vkb::Result DeviceMemoryAllocation::bind_to(vkb::Buffer buffer) const {
    vkb::BindBufferMemoryInfo bind_info{
        .sType = vkb::StructureType::BindBufferMemoryInfo,
        .buffer = buffer,
        .memory = m_device_memory,
        .memoryOffset = !is_dedicated() ? m_block->offset : 0,
    };
    return m_heap->context().vkBindBufferMemory2(1, &bind_info);
}

vkb::Result DeviceMemoryAllocation::bind_to(vkb::Image image) const {
    vkb::BindImageMemoryInfo bind_info{
        .sType = vkb::StructureType::BindImageMemoryInfo,
        .image = image,
        .memory = m_device_memory,
        .memoryOffset = !is_dedicated() ? m_block->offset : 0,
    };
    return m_heap->context().vkBindImageMemory2(1, &bind_info);
}

void DeviceMemoryAllocation::swap(DeviceMemoryAllocation &other) {
    vull::swap(m_heap, other.m_heap);
    vull::swap(m_device_memory, other.m_device_memory);
    vull::swap(m_pool, other.m_pool);
    vull::swap(m_block, other.m_block);
    vull::swap(m_mapped_data, other.m_mapped_data);
}

DeviceMemoryPool::DeviceMemoryPool(const Context &context, vkb::DeviceMemory memory, vkb::DeviceSize size,
                                   void *mapped_data)
    : m_context(context), m_memory(memory), m_mapped_data(mapped_data), m_pool(static_cast<uint32_t>(size)) {}

DeviceMemoryPool::~DeviceMemoryPool() {
    m_context.vkFreeMemory(m_memory);
}

Tuple<MemoryBlock *, void *> DeviceMemoryPool::allocate(vkb::DeviceSize size, vkb::DeviceSize alignment) {
    // Limit the alignment to a sane level so that the higher level allocator can fallback to a dedication allocation.
    constexpr auto alignment_limit = vkb::DeviceSize(1024) * 1024 * 4;
    if (size >= m_pool.total_size() || alignment > alignment_limit) {
        return vull::make_tuple<MemoryBlock *, void *>(nullptr, nullptr);
    }

    auto *block = m_pool.allocate(static_cast<uint32_t>(size), static_cast<uint32_t>(alignment));
    void *mapped_data = nullptr;
    if (block != nullptr && m_mapped_data != nullptr) {
        mapped_data = static_cast<uint8_t *>(m_mapped_data) + block->offset;
    }
    return vull::make_tuple(block, mapped_data);
}

void DeviceMemoryPool::free(MemoryBlock *block) {
    m_pool.free(block);
}

bool DeviceMemoryPool::is_empty() const {
    return m_pool.used_size() == 0;
}

vkb::Result DeviceMemoryHeap::allocate_device_memory(vkb::DeviceSize size, vkb::Buffer dedicated_buffer,
                                                     vkb::Image dedicated_image, float priority,
                                                     vkb::DeviceMemory *memory, void **mapped_data) {
    // TODO: Check budgets with VK_EXT_memory_budget.
    tracing::ScopedTrace trace("vkAllocateMemory");

    vkb::MemoryPriorityAllocateInfoEXT priority_info{
        .sType = vkb::StructureType::MemoryPriorityAllocateInfoEXT,
        .priority = priority,
    };
    vkb::MemoryDedicatedAllocateInfo dedicated_info{
        .sType = vkb::StructureType::MemoryDedicatedAllocateInfo,
        .pNext = &priority_info,
        .image = dedicated_image,
        .buffer = dedicated_buffer,
    };
    vkb::MemoryAllocateFlagsInfo flags_info{
        .sType = vkb::StructureType::MemoryAllocateFlagsInfo,
        .pNext = &dedicated_info,
    };

    // Enable device address support if this is either a generic memory block or a dedicated buffer allocation.
    if (dedicated_image == nullptr) {
        flags_info.flags = vkb::MemoryAllocateFlags::DeviceAddress;
    }

    vkb::MemoryAllocateInfo memory_ai{
        .sType = vkb::StructureType::MemoryAllocateInfo,
        .pNext = &flags_info,
        .allocationSize = size,
        .memoryTypeIndex = m_memory_type_index,
    };
    if (auto result = m_context.vkAllocateMemory(&memory_ai, memory); result != vkb::Result::Success) {
        return result;
    }

    if (m_is_mappable) {
        if (auto result = m_context.vkMapMemory(*memory, 0, vkb::k_whole_size, 0, mapped_data);
            result != vkb::Result::Success) {
            vull::error("[vulkan] Failed to map device memory block of size {}", size);
            m_context.vkFreeMemory(*memory);
            return result;
        }
    }
    return vkb::Result::Success;
}

Optional<DeviceMemoryAllocation> DeviceMemoryHeap::allocate(vkb::DeviceSize size, vkb::DeviceSize alignment,
                                                            vkb::Buffer dedicated_buffer, vkb::Image dedicated_image,
                                                            float dedicated_priority) {
    // Try to allocate dedicated if desired.
    if (dedicated_buffer != nullptr || dedicated_image != nullptr) {
        vkb::DeviceMemory memory;
        void *mapped_data;
        if (allocate_device_memory(size, dedicated_buffer, dedicated_image, dedicated_priority, &memory,
                                   &mapped_data) == vkb::Result::Success) {
            return DeviceMemoryAllocation(this, memory, mapped_data);
        }
    }

    // This lock currently prevents both concurrent pool access and multiple pool creation. This isn't optimal but won't
    // be until there is a tasklet shared mutex.
    // TODO: Make this a shared mutex, and have a separate normal mutex per pool.
    ScopedLock lock(m_mutex);

    // Attempt to allocate from the existing pools.
    // TODO: Round alignment up to nonCoherentAtomSize if heap memory type is host visible and not coherent.
    for (auto &pool : m_pools) {
        auto [block, data] = pool->allocate(size, alignment);
        if (block != nullptr) {
            return DeviceMemoryAllocation(this, pool->memory(), pool.ptr(), block, data);
        }
    }

    // Try to create a new pool and allocate from it.
    for (uint32_t shift = 0; shift < 5; shift++) {
        const auto attempt_size = m_pool_size >> shift;
        vkb::DeviceMemory memory;
        void *mapped_data;
        if (allocate_device_memory(attempt_size, nullptr, nullptr, 0.5f, &memory, &mapped_data) ==
            vkb::Result::Success) {
            vull::debug("[vulkan] Created new pool of size {} for memory type {}", attempt_size, m_memory_type_index);

            // Create a new pool and try to allocate from it before we put it in the pool list.
            auto pool = vull::make_unique<DeviceMemoryPool>(m_context, memory, attempt_size, mapped_data);
            auto [block, data] = pool->allocate(size, alignment);
            if (block != nullptr) {
                auto *pool_ptr = m_pools.emplace(vull::move(pool)).ptr();
                return DeviceMemoryAllocation(this, memory, pool_ptr, block, data);
            }
            m_pools.push(vull::move(pool));
            break;
        }
    }

    // We can unlock the mutex now since the pools won't be touched again.
    lock.unlock();

    // Finally try a dedicated allocation if we didn't already.
    if (dedicated_buffer == nullptr && dedicated_image == nullptr) {
        vkb::DeviceMemory memory;
        void *mapped_data;
        if (allocate_device_memory(size, nullptr, nullptr, dedicated_priority, &memory, &mapped_data) ==
            vkb::Result::Success) {
            return DeviceMemoryAllocation(this, memory, mapped_data);
        }
    }

    // Otherwise we are out of memory.
    return vull::nullopt;
}

uint32_t DeviceMemoryHeap::find_pool_index(DeviceMemoryPool *pool) const {
    for (uint32_t i = 0; i < m_pools.size(); i++) {
        if (m_pools[i].ptr() == pool) {
            return i;
        }
    }
    VULL_ENSURE_NOT_REACHED();
}

void DeviceMemoryHeap::free(const DeviceMemoryAllocation &allocation) {
    if (allocation.is_dedicated()) {
        m_context.vkFreeMemory(allocation.device_memory());
        return;
    }

    ScopedLock lock(m_mutex);

    // See if we already have an empty pool before freeing our allocation.
    const bool already_have_empty_pool = vull::find_if(m_pools.begin(), m_pools.end(), [](auto &pool) {
        return pool->is_empty();
    }) != m_pools.end();

    // Free the allocation.
    auto *pool = allocation.pool();
    pool->free(allocation.block());

    // See if we can free an unused pool.
    UniquePtr<DeviceMemoryPool> pool_to_free;
    if (pool->is_empty() && already_have_empty_pool) {
        vull::swap(m_pools[find_pool_index(pool)], m_pools.last());
        pool_to_free = m_pools.take_last();
    } else if (already_have_empty_pool && m_pools.last()->is_empty()) {
        pool_to_free = m_pools.take_last();
    }

    lock.unlock();
    if (pool_to_free) {
        vull::debug("[vulkan] Deleted empty pool of size {} for memory type {}", pool_to_free->size(),
                    m_memory_type_index);
    }
    pool_to_free.clear();
}

DeviceMemoryAllocator::DeviceMemoryAllocator(Context &context) : m_context(context) {
    vkb::PhysicalDeviceVulkan11Properties vulkan_11_properties{
        .sType = vkb::StructureType::PhysicalDeviceVulkan11Properties,
    };
    vkb::PhysicalDeviceProperties2 device_properties{
        .sType = vkb::StructureType::PhysicalDeviceProperties2,
        .pNext = &vulkan_11_properties,
    };
    context.vkGetPhysicalDeviceProperties2(&device_properties);
    m_buffer_image_granularity = device_properties.properties.limits.bufferImageGranularity;
    m_max_memory_allocation_size = vulkan_11_properties.maxMemoryAllocationSize;

    // TODO: Respect this when making dedication allocations.
    m_max_memory_allocation_count = device_properties.properties.limits.maxMemoryAllocationCount;

    vkb::PhysicalDeviceMemoryProperties2 memory_properties{
        .sType = vkb::StructureType::PhysicalDeviceMemoryProperties2,
    };
    context.vkGetPhysicalDeviceMemoryProperties2(&memory_properties);
    m_memory_properties = memory_properties.memoryProperties;

    // Create a heap for each memory type index in order for now.
    for (uint32_t memory_type_index = 0; memory_type_index < m_memory_properties.memoryTypeCount; memory_type_index++) {
        m_heaps.push(create_heap(memory_type_index));
    }
}

UniquePtr<DeviceMemoryHeap> DeviceMemoryAllocator::create_heap(uint32_t memory_type_index) {
    // Retrieve memory type info from the device.
    const auto &memory_type = m_memory_properties.memoryTypes[memory_type_index];
    const auto heap_size = m_memory_properties.memoryHeaps[memory_type.heapIndex].size;
    const auto property_flags = memory_type.propertyFlags;

    // Decide on the default pool size. Default to 1/8th of the heap size, clamped to the implementation's upper limit
    // and 256 MiB.
    constexpr auto max_pool_size = vkb::DeviceSize(256) * 1024 * 1024;
    const auto pool_size = vull::min(heap_size >> 3, vull::min(max_pool_size, m_max_memory_allocation_size));

    const bool is_mappable = (property_flags & vkb::MemoryPropertyFlags::HostVisible) != vkb::MemoryPropertyFlags::None;
    return vull::make_unique<DeviceMemoryHeap>(m_context, memory_type_index, pool_size, is_mappable);
}

// TODO: This should be generated in vulkan.hh
static vkb::MemoryPropertyFlags operator~(vkb::MemoryPropertyFlags flags) {
    return static_cast<vkb::MemoryPropertyFlags>(~static_cast<uint32_t>(flags));
}

// TODO: Write unit tests for this.
Optional<uint32_t> DeviceMemoryAllocator::find_best_type_index(DeviceMemoryFlags flags,
                                                               const uint32_t memory_type_bits) const {
    // Devalue all unknown flags by default.
    auto undesirable_flags = static_cast<vkb::MemoryPropertyFlags>(UINT32_MAX);
    undesirable_flags &= ~vkb::MemoryPropertyFlags::DeviceLocal;
    undesirable_flags &= ~vkb::MemoryPropertyFlags::HostVisible;
    undesirable_flags &= ~vkb::MemoryPropertyFlags::HostCoherent;
    undesirable_flags &= ~vkb::MemoryPropertyFlags::HostCached;

    // Build sets of flags.
    auto required_flags = vkb::MemoryPropertyFlags::None;
    auto desirable_flags = vkb::MemoryPropertyFlags::None;
    if (flags.is_set(DeviceMemoryFlag::HostRandomAccess)) {
        // Prefer cached to improve read speed and BAR memory for readback.
        required_flags |= vkb::MemoryPropertyFlags::HostVisible;
        desirable_flags |= vkb::MemoryPropertyFlags::HostCached | vkb::MemoryPropertyFlags::DeviceLocal;
    } else if (flags.is_set(DeviceMemoryFlag::HostSequentialWrite)) {
        required_flags |= vkb::MemoryPropertyFlags::HostVisible;
        undesirable_flags |= vkb::MemoryPropertyFlags::HostCached;
        if (flags.is_set(DeviceMemoryFlag::Staging)) {
            // Don't waste (potentially) limited BAR memory on staging buffers, which is especially sad if overcommit is
            // allowed. If ReBAR is enabled, the host-visible and non-device-local memory type should no longer exist so
            // this won't make a difference and the full performance benefit should be realized.
            // TODO: Check this behaviour on systems with ReBAR.
            undesirable_flags |= vkb::MemoryPropertyFlags::DeviceLocal;
        } else {
            desirable_flags |= vkb::MemoryPropertyFlags::DeviceLocal;
        }
    } else {
        // Just want plain VRAM.
        required_flags |= vkb::MemoryPropertyFlags::DeviceLocal;
        undesirable_flags |= vkb::MemoryPropertyFlags::HostVisible;
    }

    uint32_t best_cost = UINT32_MAX;
    Optional<uint32_t> best_type_index;
    for (uint32_t type_index = 0; type_index < m_memory_properties.memoryTypeCount; type_index++) {
        if ((memory_type_bits & (1u << type_index)) == 0u) {
            // Memory type is not usable for this allocation.
            continue;
        }

        const auto type_flags = m_memory_properties.memoryTypes[type_index].propertyFlags;
        if ((type_flags & required_flags) != required_flags) {
            // Memory type doesn't have all of the required flags.
            continue;
        }

        // Cost is all of the desirable flags we don't have plus all of the undesirable flags we do have.
        const auto cost = static_cast<uint32_t>(vull::popcount(vull::to_underlying(~type_flags & desirable_flags))) +
                          static_cast<uint32_t>(vull::popcount(vull::to_underlying(type_flags & undesirable_flags)));
        if (cost < best_cost) {
            best_cost = cost;
            best_type_index = type_index;
        }
        if (cost == 0) {
            // Perfect match.
            break;
        }
    }
    return best_type_index;
}

Optional<DeviceMemoryAllocation> DeviceMemoryAllocator::allocate_memory(vkb::DeviceSize size, vkb::DeviceSize alignment,
                                                                        DeviceMemoryFlags flags,
                                                                        uint32_t memory_type_bits, vkb::Buffer buffer,
                                                                        vkb::Image image) {
    tracing::ScopedTrace trace("Allocate VRAM");

    // Automatically prefer dedicated for large resources.
    if (size >= vkb::DeviceSize(16) * 1024 * 1024) {
        flags.set(DeviceMemoryFlag::PreferDedicated);
    }

    // The presence of these inform the heap allocation function whether to try a dedicated allocation.
    buffer = flags.is_set(DeviceMemoryFlag::PreferDedicated) ? buffer : nullptr;
    image = flags.is_set(DeviceMemoryFlag::PreferDedicated) ? image : nullptr;

    // Use default priority of 0.5 if high priority not set.
    const float dedicated_priority = flags.is_set(DeviceMemoryFlag::HighPriority) ? 1.0f : 0.5f;

    // TODO: This could potentially waste a lot of memory on non-desktop platforms and a little bit of memory on non-AMD
    //       platforms. We could do something smarter here like separate buffers and optimal images into separate pools.
    //       That would also allow a higher default memory priority for the optimal image pool.
    alignment = vull::max(alignment, m_buffer_image_granularity);

    if (tracing::is_enabled()) {
        trace.add_text(vull::format("Size: {}", size));
        trace.add_text(vull::format("Alignment: {}", alignment));
        if (flags.is_set(DeviceMemoryFlag::PreferDedicated)) {
            trace.add_text("Dedicated");
        }
    }

    // Try allocating with each acceptable memory type.
    auto type_index = find_best_type_index(flags, memory_type_bits);
    while (type_index) {
        auto allocation = m_heaps[*type_index]->allocate(size, alignment, buffer, image, dedicated_priority);
        if (allocation) [[likely]] {
            if (tracing::is_enabled()) {
                trace.add_text(vull::format("Memory Type: {}", *type_index));
            }
            return allocation;
        }

        // Try to find another suitable memory type.
        memory_type_bits &= ~(1u << *type_index);
        type_index = find_best_type_index(flags, memory_type_bits);
    }
    return vull::nullopt;
}

Optional<DeviceMemoryAllocation> DeviceMemoryAllocator::allocate_for(vkb::Buffer buffer, DeviceMemoryFlags flags) {
    vkb::BufferMemoryRequirementsInfo2 buffer_info{
        .sType = vkb::StructureType::BufferMemoryRequirementsInfo2,
        .buffer = buffer,
    };
    vkb::MemoryDedicatedRequirements dedicated_requirements{
        .sType = vkb::StructureType::MemoryDedicatedRequirements,
    };
    vkb::MemoryRequirements2 requirements{
        .sType = vkb::StructureType::MemoryRequirements2,
        .pNext = &dedicated_requirements,
    };
    m_context.vkGetBufferMemoryRequirements2(&buffer_info, &requirements);

    // We don't currently care about requiresDedicatedAllocation since it only applies to external memory.
    if (dedicated_requirements.prefersDedicatedAllocation) {
        flags.set(DeviceMemoryFlag::PreferDedicated);
    }

    const auto &memory_requirements = requirements.memoryRequirements;
    auto allocation = allocate_memory(memory_requirements.size, memory_requirements.alignment, flags,
                                      memory_requirements.memoryTypeBits, buffer, nullptr);
    if (!allocation) {
        return vull::nullopt;
    }
    if (allocation->bind_to(buffer) != vkb::Result::Success) {
        return vull::nullopt;
    }
    return allocation;
}

Optional<DeviceMemoryAllocation> DeviceMemoryAllocator::allocate_for(vkb::Image image, DeviceMemoryFlags flags) {
    vkb::ImageMemoryRequirementsInfo2 image_info{
        .sType = vkb::StructureType::ImageMemoryRequirementsInfo2,
        .image = image,
    };
    vkb::MemoryDedicatedRequirements dedicated_requirements{
        .sType = vkb::StructureType::MemoryDedicatedRequirements,
    };
    vkb::MemoryRequirements2 requirements{
        .sType = vkb::StructureType::MemoryRequirements2,
        .pNext = &dedicated_requirements,
    };
    m_context.vkGetImageMemoryRequirements2(&image_info, &requirements);

    // We don't currently care about requiresDedicatedAllocation since it only applies to external memory or DRM
    // formats.
    if (dedicated_requirements.prefersDedicatedAllocation) {
        flags.set(DeviceMemoryFlag::PreferDedicated);
    }

    const auto &memory_requirements = requirements.memoryRequirements;
    auto allocation = allocate_memory(memory_requirements.size, memory_requirements.alignment, flags,
                                      memory_requirements.memoryTypeBits, nullptr, image);
    if (!allocation) {
        return vull::nullopt;
    }
    if (allocation->bind_to(image) != vkb::Result::Success) {
        return vull::nullopt;
    }
    return allocation;
}

} // namespace vull::vk
