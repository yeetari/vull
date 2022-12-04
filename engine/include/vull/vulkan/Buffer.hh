#pragma once

#include <vull/vulkan/Allocation.hh>
#include <vull/vulkan/Vulkan.hh>

namespace vull::vk {

class Context;

class Buffer {
    friend Context;

private:
    Allocation m_allocation;
    vkb::Buffer m_buffer{nullptr};
    vkb::DeviceAddress m_device_address{0};

    Buffer(Allocation &&allocation, vkb::Buffer buffer, vkb::BufferUsage usage);

public:
    Buffer() = default;
    Buffer(const Buffer &) = delete;
    Buffer(Buffer &&);
    ~Buffer();

    Buffer &operator=(const Buffer &) = delete;
    Buffer &operator=(Buffer &&);

    vkb::Buffer operator*() const { return m_buffer; }
    vkb::DeviceAddress device_address() const { return m_device_address; }
    void *mapped_raw() const { return m_allocation.mapped_data(); }
    template <typename T>
    T *mapped() const;
};

template <typename T>
T *Buffer::mapped() const {
    return static_cast<T *>(mapped_raw());
}

} // namespace vull::vk
