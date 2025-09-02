#include <vull/vulkan/memory.hh>

#include <vull/container/array.hh>
#include <vull/core/log.hh>
#include <vull/maths/common.hh>
#include <vull/support/assert.hh>
#include <vull/support/tuple.hh>
#include <vull/support/utility.hh>

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

        // Check if we can append the space straight to the previous block.
        if (auto *prev = block->prev_phys; prev->is_free && prev->offset < block->offset) {
            // We need to relink the block if its size increases enough to move up a size class.
            const auto [old_fl_index, old_sl_index] = size_mapping(prev->size);
            const auto [new_fl_index, new_sl_index] = size_mapping(prev->size + padding);
            if (old_fl_index != new_fl_index || old_sl_index != new_sl_index) {
                unlink_block(prev, old_fl_index, old_sl_index);
                prev->size += padding;
                link_block(prev);
            } else {
                prev->size += padding;
            }
        } else {
            // Otherwise create a new block for the padding.
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
        }

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

} // namespace vull::vk
