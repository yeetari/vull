#include <vull/vulkan/Allocator.hh>

#include <vull/core/Log.hh>
#include <vull/maths/Common.hh>
#include <vull/support/Array.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Optional.hh>
#include <vull/support/UniquePtr.hh>
#include <vull/support/Utility.hh>
#include <vull/support/Vector.hh>
#include <vull/vulkan/Allocation.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stdint.h>

// An implementation of the TLSF memory allocation algorithm for video memory. Each Allocator manages a specific vulkan
// memory type (e.g. HostVisible | HostCoherent), as well as a number of fixed-size heaps (n.b. the Heap class defined
// below, not a full vulkan memory heap). The Heap class owns a fixed-size vkb::DeviceMemory object, and implements TLSF
// for subdividing the device memory chunk into smaller blocks. TLSF works using a two-tiered hierarchy of block size
// buckets. The first level is spread across power-of-twos. Each first level is made up of k_sl_count second levels,
// which has linearly spaced block sizes. Each first level has a bitset representing which second levels have one or
// more free blocks available for use. There is also a single bitset representing which first levels have one or more
// free second levels available.

// Each first and second level index pair represents a size bucket for blocks. Each bucket contains a linked list of
// free blocks available to be allocated (the list links are actually stored in the block header, with an array in the
// allocator to mark list heads). Upon allocation, a suitable bucket is found from the desired size. Since we have
// bitsets indicating which free blocks are available, this can be done without having to check the free list.

// Each block is also part of a circular linked list called the physical list, which retains the address order of the
// allocated memory. It is used to coalesce free neighbouring blocks when a block is freed.

// Since this is an external allocator (i.e. not one that manages normal RAM, where a block header can be placed before
// or after the returned allocation bytes), the block header memory needs to be managed separately, which is currently
// done by manual new and delete.
// TODO: This can probably be improved.

namespace vull::vk {
namespace {

using Bitset = uint32_t;
constexpr uint32_t k_align_log2 = vull::log2(256u);
constexpr uint32_t k_sl_count_log2 = vull::log2(sizeof(Bitset) * 8u);
constexpr uint32_t k_sl_count = 1u << k_sl_count_log2;
constexpr uint32_t k_fl_offset = k_sl_count_log2 + k_align_log2;
constexpr uint32_t k_fl_count = k_sl_count - k_fl_offset + 1u;
constexpr uint32_t k_minimum_allocation_size = 1u << k_align_log2;

constexpr vkb::DeviceSize k_big_heap_size = 128 * 1024 * 1024;
constexpr vkb::DeviceSize k_small_heap_cutoff = 1024 * 1024 * 1024;

struct Block {
    Block *prev_free;
    Block *next_free;
    Block *prev_phys;
    Block *next_phys;
    uint32_t offset;
    uint32_t size; // LSb == free flag
};

struct BlockMapping {
    uint32_t fl_index;
    uint32_t sl_index;
};

BlockMapping mapping(uint32_t size) {
    const auto fl_index = vull::log2(size);
    const auto sl_index = (size >> (fl_index - k_sl_count_log2)) ^ k_sl_count;
    return {fl_index - k_align_log2, sl_index};
}

} // namespace

// Individual memory heap that manages k_heap_size VRAM.
class Heap {
    const vkb::DeviceMemory m_memory;
    void *const m_mapped_data;
    Bitset m_fl_bitset{};
    Array<Bitset, k_fl_count> m_sl_bitsets{};
    Array<Array<Block *, k_sl_count>, k_fl_count> m_block_map{};

    void link_block(Block *block);
    void unlink_block(const Block *block, uint32_t fl_index, uint32_t sl_index);

public:
    Heap(vkb::DeviceMemory memory, vkb::DeviceSize size, void *mapped_data);
    Heap(const Heap &) = delete;
    Heap(Heap &&) = delete;
    ~Heap();

    Heap &operator=(const Heap &) = delete;
    Heap &operator=(Heap &&) = delete;

    Optional<AllocationInfo> allocate(uint32_t);
    void free(const AllocationInfo &);

    vkb::DeviceMemory memory() const { return m_memory; }
    void *mapped_data() const { return m_mapped_data; }
};

Heap::Heap(vkb::DeviceMemory memory, vkb::DeviceSize size, void *mapped_data)
    : m_memory(memory), m_mapped_data(mapped_data) {
    // Create initial block with full size.
    auto *block = new Block{
        .size = static_cast<uint32_t>(size) | 1u,
    };
    block->prev_phys = block;
    block->next_phys = block;
    link_block(block);
}

Heap::~Heap() {
    for (auto &sl : m_block_map) {
        for (auto *block : sl) {
            if (block != nullptr) {
                VULL_ASSERT(block->prev_free == nullptr && block->next_free == nullptr);
                VULL_ASSERT(block->prev_phys == block && block->next_phys == block);
            }
            delete block;
        }
    }
}

// Insert a block into its bucket's free list and make it the head.
void Heap::link_block(Block *block) {
    const auto [fl_index, sl_index] = mapping(block->size & ~1u);
    block->prev_free = nullptr;
    block->next_free = vull::exchange(m_block_map[fl_index][sl_index], block);
    if (block->next_free != nullptr) {
        block->next_free->prev_free = block;
    }
    m_fl_bitset |= (1ull << fl_index);
    m_sl_bitsets[fl_index] |= (1ull << sl_index);
}

// Unlink block from free list.
void Heap::unlink_block(const Block *block, uint32_t fl_index, uint32_t sl_index) {
    auto *prev_free = block->prev_free;
    auto *next_free = block->next_free;
    VULL_ASSERT(prev_free != nullptr || next_free != nullptr || m_block_map[fl_index][sl_index] == block);

    if (prev_free != nullptr) {
        prev_free->next_free = next_free;
    }
    if (next_free != nullptr) {
        next_free->prev_free = prev_free;
    }

    if (m_block_map[fl_index][sl_index] != block) {
        // Block wasn't head of free list.
        return;
    }

    // Update list head.
    m_block_map[fl_index][sl_index] = next_free;
    if (next_free == nullptr) {
        // Last free block in list, clear bit in second level.
        m_sl_bitsets[fl_index] &= ~(1ull << sl_index);
        if (m_sl_bitsets[fl_index] == 0) {
            // Last free block in second level, clear bit in first level.
            m_fl_bitset &= ~(1ull << fl_index);
        }
    }
}

Optional<AllocationInfo> Heap::allocate(uint32_t size) {
    // Round up to minimum allocation size (minimum alignment).
    size = vull::align_up(size, k_minimum_allocation_size);

    // Round up to next block size.
    size = vull::align_up(size, 1u << (vull::log2(size) - k_sl_count_log2));

    auto [fl_index, sl_index] = mapping(size);
    auto sl_bitset = m_sl_bitsets[fl_index] & (~0u << sl_index);
    if (sl_bitset == 0) {
        // Second level exhausted, move up to the next first level.
        const auto fl_bitset = m_fl_bitset & (~0u << (fl_index + 1));
        if (fl_bitset == 0) {
            // First level exhausted, heap is full.
            return {};
        }
        fl_index = vull::ffs(fl_bitset);
        sl_bitset = m_sl_bitsets[fl_index];
    }
    sl_index = vull::ffs(sl_bitset);

    auto *block = m_block_map[fl_index][sl_index];
    VULL_ASSERT((block->size & 1u) == 1u, "Attempted allocation of non-free block");

    // Clear free flag. It's now safe to use block.size directly after this point.
    block->size &= ~1u;
    unlink_block(block, fl_index, sl_index);

    VULL_ASSERT(block->size >= size);
    if (block->size - size >= k_minimum_allocation_size) {
        // Block is big enough to split, resize 'block' to be the size of the allocation (note not the exact size of the
        // allocation at this point, rather the rounded-up size) and create a new block for the remainder of the free
        // space.
        auto *remainder_block = new Block{
            .offset = block->offset + size,
            .size = (block->size - size) | 1u,
        };
        block->size = size;

        // Update physical linked list to place the new remainder block after our allocated block.
        remainder_block->prev_phys = block;
        remainder_block->next_phys = vull::exchange(block->next_phys, remainder_block);
        remainder_block->next_phys->prev_phys = remainder_block;
        link_block(remainder_block);
    }

    return AllocationInfo{
        .memory = m_memory,
        .block = block,
        .offset = block->offset,
    };
}

void Heap::free(const AllocationInfo &allocation) {
    auto *block = static_cast<Block *>(allocation.block);
    VULL_ASSERT((block->size & 1u) == 0u, "Block already free");

    // Try to coalesce free neighbouring blocks. The offset check is needed as the physical list is circular.
    if (auto *prev = block->prev_phys; (prev->size & 1u) == 1u && prev->offset < block->offset) {
        prev->size &= ~1u;
        const auto [fl_index, sl_index] = mapping(prev->size);
        unlink_block(prev, fl_index, sl_index);

        // Consume previous block into 'block'.
        block->offset -= prev->size;
        block->size += prev->size;
        block->prev_phys = prev->prev_phys;
        block->prev_phys->next_phys = block;
        delete prev;
    }
    if (auto *next = block->next_phys; (next->size & 1u) == 1u && next->offset > block->offset) {
        next->size &= ~1u;
        const auto [fl_index, sl_index] = mapping(next->size);
        unlink_block(next, fl_index, sl_index);

        // Consume next block into 'block'.
        block->size += next->size;
        block->next_phys = next->next_phys;
        block->next_phys->prev_phys = block;
        delete next;
    }

    // Remark the block as free and insert it back into its bucket's free list.
    block->size |= 1u;
    link_block(block);
}

Allocator::Allocator(Context &context, uint32_t memory_type_index)
    : m_context(context), m_memory_type_index(memory_type_index), m_heap_size(k_big_heap_size) {
    vkb::PhysicalDeviceMemoryProperties memory_properties{};
    context.vkGetPhysicalDeviceMemoryProperties(&memory_properties);

    const auto &memory_type = memory_properties.memoryTypes[memory_type_index];
    const auto heap_size = memory_properties.memoryHeaps[memory_type.heapIndex].size;
    if (heap_size <= k_small_heap_cutoff) {
        m_heap_size = heap_size / 8;
    }
    m_heap_size = vull::align_up(m_heap_size, vkb::DeviceSize(32));
    m_mappable = (memory_type.propertyFlags & vkb::MemoryPropertyFlags::HostVisible) != vkb::MemoryPropertyFlags::None;
}

Allocator::~Allocator() {
    for (auto &heap : m_heaps) {
        m_context.vkFreeMemory(heap->memory());
    }
}

Allocation Allocator::allocate_dedicated(uint32_t size) {
    // TODO: VkMemoryDedicatedAllocateInfo.
    vkb::MemoryAllocateFlagsInfo flags_info{
        .sType = vkb::StructureType::MemoryAllocateFlagsInfo,
        .flags = vkb::MemoryAllocateFlags::DeviceAddress,
    };
    vkb::MemoryAllocateInfo memory_ai{
        .sType = vkb::StructureType::MemoryAllocateInfo,
        .pNext = &flags_info,
        .allocationSize = size,
        .memoryTypeIndex = m_memory_type_index,
    };
    vkb::DeviceMemory memory;
    VULL_ENSURE(m_context.vkAllocateMemory(&memory_ai, &memory) == vkb::Result::Success);

    void *mapped_data = nullptr;
    if (m_mappable) {
        VULL_ENSURE(m_context.vkMapMemory(memory, 0, vkb::k_whole_size, 0, &mapped_data) == vkb::Result::Success);
    }

    AllocationInfo info{
        .memory = memory,
        .mapped_data = mapped_data,
        .heap_index = 0xffu,
    };
    return {*this, info};
}

// TODO: Avoid having individual heaps of N bytes? A new TLSF block can be created, but how would the backing
//       VkDeviceMemory be managed?
Allocation Allocator::allocate(const vkb::MemoryRequirements &requirements) {
    VULL_ASSERT((requirements.memoryTypeBits & (1u << m_memory_type_index)) != 0u);

    // TODO: Handle bufferImageGranularity.
    const auto alignment = vull::max(static_cast<uint32_t>(requirements.alignment), 1u);
    auto size = static_cast<uint32_t>(requirements.size);
    if (size >= m_heap_size >> 3) {
        // TODO: Check against maxMemoryAllocationCount.
        return allocate_dedicated(size);
    }

    const auto remainder = size % alignment;
    if (remainder != 0) {
        size = size + alignment - remainder;
    }

    Optional<AllocationInfo> allocation_info;
    for (uint32_t i = 0; i < m_heaps.size(); i++) {
        if ((allocation_info = m_heaps[i]->allocate(size))) {
            allocation_info->heap_index = static_cast<uint8_t>(i);
            break;
        }
    }

    for (uint32_t shift = 0; !allocation_info && shift < 6; shift++) {
        vkb::MemoryAllocateFlagsInfo flags_info{
            .sType = vkb::StructureType::MemoryAllocateFlagsInfo,
            .flags = vkb::MemoryAllocateFlags::DeviceAddress,
        };
        vkb::MemoryAllocateInfo memory_ai{
            .sType = vkb::StructureType::MemoryAllocateInfo,
            .pNext = &flags_info,
            .allocationSize = m_heap_size >> shift,
            .memoryTypeIndex = m_memory_type_index,
        };
        vkb::DeviceMemory memory;
        if (const auto result = m_context.vkAllocateMemory(&memory_ai, &memory); result != vkb::Result::Success) {
            VULL_ENSURE(result == vkb::Result::ErrorOutOfDeviceMemory);
            continue;
        }
        vull::trace("[vulkan] New heap of size {} created for memory type {}", m_heap_size >> shift,
                    m_memory_type_index);

        void *mapped_data = nullptr;
        if (m_mappable) {
            VULL_ENSURE(m_context.vkMapMemory(memory, 0, vkb::k_whole_size, 0, &mapped_data) == vkb::Result::Success);
        }

        auto &heap = m_heaps.emplace(new Heap(memory, m_heap_size >> shift, mapped_data));
        allocation_info = heap->allocate(size);
        allocation_info->heap_index = static_cast<uint8_t>(m_heaps.size() - 1);
    }

    VULL_ENSURE(allocation_info);
    allocation_info->offset = vull::align_up(allocation_info->offset, alignment);
    if (auto *mapped_base = m_heaps[allocation_info->heap_index]->mapped_data()) {
        allocation_info->mapped_data = static_cast<uint8_t *>(mapped_base) + allocation_info->offset;
    }
    return {*this, *allocation_info};
}

void Allocator::free(const Allocation &allocation) {
    if (allocation.is_dedicated()) {
        m_context.vkFreeMemory(allocation.info().memory);
        return;
    }
    m_heaps[allocation.info().heap_index]->free(allocation.info());
    // TODO: Shrink heaps based on heuristic.
}

Allocation::~Allocation() {
    if (m_allocator != nullptr) {
        m_allocator->free(*this);
    }
    m_allocator = nullptr;
}

Allocation &Allocation::operator=(Allocation &&other) {
    Allocation moved(vull::move(other));
    vull::swap(m_allocator, moved.m_allocator);
    m_info = moved.m_info;
    return *this;
}

} // namespace vull::vk
