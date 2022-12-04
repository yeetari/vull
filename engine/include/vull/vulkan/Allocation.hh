#pragma once

#include <vull/support/Utility.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stdint.h>

namespace vull::vk {

class Allocator;
using BlockIndex = uint16_t;

struct AllocationInfo {
    vkb::DeviceMemory memory;
    uint32_t offset{0};
    BlockIndex block_index{0};
    void *mapped_data{nullptr};
    uint8_t heap_index;
};

class Allocation {
    friend Allocator;

private:
    Allocator *m_allocator{nullptr};
    AllocationInfo m_info;

    Allocation(Allocator &allocator, const AllocationInfo &info) : m_allocator(&allocator), m_info(info) {}

public:
    Allocation() = default;
    Allocation(const Allocation &) = delete;
    Allocation(Allocation &&other) : m_allocator(vull::exchange(other.m_allocator, nullptr)), m_info(other.m_info) {}
    ~Allocation();

    Allocation &operator=(const Allocation &) = delete;
    Allocation &operator=(Allocation &&);

    const Allocator *allocator() const { return m_allocator; }
    const AllocationInfo &info() const { return m_info; }
    void *mapped_data() const { return m_info.mapped_data; }
    bool is_dedicated() const { return m_info.heap_index == 0xffu; }
};

} // namespace vull::vk
