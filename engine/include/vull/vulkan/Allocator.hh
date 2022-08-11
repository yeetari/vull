#pragma once

#include <vull/support/UniquePtr.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stdint.h>

namespace vull::vk {

class AllocatorImpl;
class Context;

using BlockIndex = uint16_t;

struct Allocation {
    vkb::DeviceMemory memory;
    uint32_t offset;
    BlockIndex block_index;
    uint8_t heap_index;
    bool dedicated;
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

    Allocation allocate(const vkb::MemoryRequirements &requirements);
    Allocation bind_memory(vkb::Buffer buffer);
    Allocation bind_memory(vkb::Image image);
    void free(Allocation allocation);
};

} // namespace vull::vk
