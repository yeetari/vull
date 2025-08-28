#pragma once

#include <vull/support/span.hh>
#include <vull/vulkan/allocation.hh>
#include <vull/vulkan/sampler.hh>
#include <vull/vulkan/vulkan.hh>

namespace vull::vk {

class Context;
class ImageView;
class Queue;
class SampledImage;

class Buffer {
    friend Context;

private:
    Allocation m_allocation;
    vkb::Buffer m_buffer{nullptr};
    vkb::BufferUsage m_usage{};
    vkb::DeviceAddress m_device_address{0};
    vkb::DeviceSize m_size{0};

    Buffer(Allocation &&allocation, vkb::Buffer buffer, vkb::BufferUsage usage, vkb::DeviceSize size);

public:
    Buffer() = default;
    Buffer(const Buffer &) = delete;
    Buffer(Buffer &&);
    ~Buffer();

    Buffer &operator=(const Buffer &) = delete;
    Buffer &operator=(Buffer &&);

    Buffer create_staging() const;
    void copy_from(const Buffer &src, Queue &queue) const;
    void upload(Span<const void> data) const;

    void set_descriptor(vkb::DescriptorSetLayout layout, uint32_t binding, uint32_t element, Sampler sampler) const;
    void set_descriptor(vkb::DescriptorSetLayout layout, uint32_t binding, uint32_t element,
                        const Buffer &buffer) const;
    void set_descriptor(vkb::DescriptorSetLayout layout, uint32_t binding, uint32_t element,
                        const SampledImage &image) const;
    void set_descriptor(vkb::DescriptorSetLayout layout, uint32_t binding, uint32_t element,
                        const ImageView &view) const;

    Context &context() const;
    vkb::Buffer operator*() const { return m_buffer; }
    vkb::BufferUsage usage() const { return m_usage; }
    vkb::DeviceAddress device_address() const;
    vkb::DeviceSize size() const { return m_size; }
    void *mapped_raw() const { return m_allocation.mapped_data(); }
    template <typename T>
    T *mapped() const;
};

template <typename T>
T *Buffer::mapped() const {
    return static_cast<T *>(mapped_raw());
}

} // namespace vull::vk
