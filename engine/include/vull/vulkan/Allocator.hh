#pragma once

#include <vull/support/UniquePtr.hh>
#include <vull/support/Utility.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stdint.h>

namespace vull::vk {

class AllocatorImpl;
class Context;

using BlockIndex = uint16_t;

struct AllocationInfo {
    vkb::DeviceMemory memory;
    uint32_t offset{0};
    BlockIndex block_index{0};
    void *mapped_data{nullptr};
    uint8_t heap_index;
};

class Allocation {
    friend AllocatorImpl;

private:
    AllocatorImpl *m_allocator{nullptr};
    AllocationInfo m_info;

    Allocation(AllocatorImpl &allocator, const AllocationInfo &info) : m_allocator(&allocator), m_info(info) {}

public:
    Allocation() = default;
    Allocation(const Allocation &) = delete;
    Allocation(Allocation &&other) : m_allocator(vull::exchange(other.m_allocator, nullptr)), m_info(other.m_info) {}
    ~Allocation();

    Allocation &operator=(const Allocation &) = delete;
    Allocation &operator=(Allocation &&);

    const AllocationInfo &info() const { return m_info; }
    void *mapped_data() const { return m_info.mapped_data; }
    bool is_dedicated() const { return m_info.heap_index == 0xffu; }
};

class Allocator {
    UniquePtr<AllocatorImpl> m_impl;

public:
    Allocator();
    Allocator(const Context &context, uint32_t memory_type_index);
    Allocator(const Allocator &) = delete;
    Allocator(Allocator &&);
    ~Allocator();

    Allocator &operator=(const Allocator &) = delete;
    Allocator &operator=(Allocator &&);

    [[nodiscard]] Allocation allocate(const vkb::MemoryRequirements &requirements);
    uint32_t memory_type_index() const;
};

} // namespace vull::vk
