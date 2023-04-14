#pragma once

#include <vull/container/Vector.hh>
#include <vull/support/UniquePtr.hh> // IWYU pragma: keep
#include <vull/tasklet/Mutex.hh>
#include <vull/vulkan/Allocation.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stdint.h>

namespace vull::vk {

class Context;
class Heap;

struct HeapRange {
    uint32_t start;
    uint32_t size;
    bool free;
};

class Allocator {
    friend Allocation; // for free()

private:
    Context &m_context;
    const uint32_t m_memory_type_index;
    Vector<UniquePtr<Heap>> m_heaps;
    vkb::DeviceSize m_heap_size;
    mutable Mutex m_mutex;
    bool m_mappable{false};

    Allocation allocate_dedicated(uint32_t size);
    void free(const Allocation &allocation);

public:
    Allocator(Context &context, uint32_t memory_type_index);
    Allocator(const Allocator &) = delete;
    Allocator(Allocator &&) = delete;
    ~Allocator();

    Allocator &operator=(const Allocator &) = delete;
    Allocator &operator=(Allocator &&) = delete;

    [[nodiscard]] Allocation allocate(const vkb::MemoryRequirements &requirements);
    Vector<Vector<HeapRange>> heap_ranges() const;

    Context &context() const { return m_context; }
    uint32_t memory_type_index() const { return m_memory_type_index; }
    uint32_t heap_count() const { return m_heaps.size(); }
    vkb::DeviceSize heap_size() const { return m_heap_size; }
};

} // namespace vull::vk
