#include <vull/vulkan/buffer.hh>

#include <vull/maths/common.hh>
#include <vull/support/assert.hh>
#include <vull/support/span.hh>
#include <vull/support/unique_ptr.hh>
#include <vull/support/utility.hh>
#include <vull/tasklet/future.hh>
#include <vull/vulkan/allocation.hh>
#include <vull/vulkan/allocator.hh>
#include <vull/vulkan/command_buffer.hh>
#include <vull/vulkan/context.hh>
#include <vull/vulkan/image.hh>
#include <vull/vulkan/memory_usage.hh>
#include <vull/vulkan/queue.hh>
#include <vull/vulkan/vulkan.hh>

#include <string.h>

namespace vull::vk {

Buffer::Buffer(Allocation &&allocation, vkb::Buffer buffer, vkb::BufferUsage usage, vkb::DeviceSize size)
    : m_allocation(vull::move(allocation)), m_buffer(buffer), m_usage(usage), m_size(size) {
    if ((usage & vkb::BufferUsage::ShaderDeviceAddress) == vkb::BufferUsage::ShaderDeviceAddress) {
        vkb::BufferDeviceAddressInfo address_info{
            .sType = vkb::StructureType::BufferDeviceAddressInfo,
            .buffer = buffer,
        };
        m_device_address = m_allocation.allocator()->context().vkGetBufferDeviceAddress(&address_info);
    }
}

Buffer::Buffer(Buffer &&other) {
    m_allocation = vull::move(other.m_allocation);
    m_buffer = vull::exchange(other.m_buffer, nullptr);
    m_usage = vull::exchange(other.m_usage, {});
    m_device_address = vull::exchange(other.m_device_address, 0u);
    m_size = vull::exchange(other.m_size, 0u);
}

Buffer::~Buffer() {
    if (const auto *allocator = m_allocation.allocator()) {
        allocator->context().vkDestroyBuffer(m_buffer);
    }
}

Buffer &Buffer::operator=(Buffer &&other) {
    Buffer moved(vull::move(other));
    vull::swap(m_allocation, moved.m_allocation);
    vull::swap(m_buffer, moved.m_buffer);
    vull::swap(m_usage, moved.m_usage);
    vull::swap(m_device_address, moved.m_device_address);
    vull::swap(m_size, moved.m_size);
    return *this;
}

Buffer Buffer::create_staging() const {
    return context().create_buffer(m_size, vkb::BufferUsage::TransferSrc, vk::MemoryUsage::HostOnly);
}

void Buffer::copy_from(const Buffer &src, Queue &queue) const {
    auto cmd_buf = queue.request_cmd_buf();
    vkb::BufferCopy copy{
        .size = vull::min(src.size(), m_size),
    };
    cmd_buf->copy_buffer(src, *this, copy);
    queue.submit(vull::move(cmd_buf), {}, {}).await();
}

void Buffer::upload(Span<const void> data) const {
    VULL_ASSERT(mapped_raw() != nullptr);
    memcpy(mapped_raw(), data.data(), vull::min(data.size(), m_size));
}

void Buffer::set_descriptor(vkb::DescriptorSetLayout layout, uint32_t binding, uint32_t element,
                            Sampler sampler) const {
    vkb::DeviceSize offset;
    context().vkGetDescriptorSetLayoutBindingOffsetEXT(layout, binding, &offset);

    vkb::Sampler vk_sampler = context().get_sampler(sampler);
    const auto size = context().descriptor_size(vkb::DescriptorType::Sampler);
    vkb::DescriptorGetInfoEXT get_info{
        .sType = vkb::StructureType::DescriptorGetInfoEXT,
        .type = vkb::DescriptorType::Sampler,
        .data{
            .pSampler = &vk_sampler,
        },
    };
    context().vkGetDescriptorEXT(&get_info, size, mapped<uint8_t>() + offset + element * size);
}

void Buffer::set_descriptor(vkb::DescriptorSetLayout layout, uint32_t binding, uint32_t element,
                            const Buffer &buffer) const {
    vkb::DeviceSize offset;
    context().vkGetDescriptorSetLayoutBindingOffsetEXT(layout, binding, &offset);

    const bool is_storage = (buffer.usage() & vkb::BufferUsage::StorageBuffer) == vkb::BufferUsage::StorageBuffer;
    const bool is_uniform = (buffer.usage() & vkb::BufferUsage::UniformBuffer) == vkb::BufferUsage::UniformBuffer;
    VULL_ASSERT(is_storage ^ is_uniform);
    VULL_IGNORE(is_uniform);

    const auto type = is_storage ? vkb::DescriptorType::StorageBuffer : vkb::DescriptorType::UniformBuffer;
    const auto size = context().descriptor_size(type);
    vkb::DescriptorAddressInfoEXT address_info{
        .sType = vkb::StructureType::DescriptorAddressInfoEXT,
        .address = buffer.device_address(),
        .range = buffer.size(),
    };
    vkb::DescriptorGetInfoEXT get_info{
        .sType = vkb::StructureType::DescriptorGetInfoEXT,
        .type = type,
        .data{
            .pStorageBuffer = &address_info,
        },
    };
    context().vkGetDescriptorEXT(&get_info, size, mapped<uint8_t>() + offset + element * size);
}

void Buffer::set_descriptor(vkb::DescriptorSetLayout layout, uint32_t binding, uint32_t element,
                            const SampledImage &image) const {
    vkb::DeviceSize offset;
    context().vkGetDescriptorSetLayoutBindingOffsetEXT(layout, binding, &offset);

    const bool has_sampler = image.sampler() != nullptr;
    const auto type = has_sampler ? vkb::DescriptorType::CombinedImageSampler : vkb::DescriptorType::SampledImage;
    const auto size = context().descriptor_size(type);
    vkb::DescriptorImageInfo image_info{
        .sampler = image.sampler(),
        .imageView = *image.view(),
        .imageLayout = vkb::ImageLayout::ReadOnlyOptimal,
    };
    vkb::DescriptorGetInfoEXT get_info{
        .sType = vkb::StructureType::DescriptorGetInfoEXT,
        .type = type,
        .data{
            .pCombinedImageSampler = &image_info,
        },
    };
    context().vkGetDescriptorEXT(&get_info, size, mapped<uint8_t>() + offset + element * size);
}

void Buffer::set_descriptor(vkb::DescriptorSetLayout layout, uint32_t binding, uint32_t element,
                            const ImageView &view) const {
    vkb::DeviceSize offset;
    context().vkGetDescriptorSetLayoutBindingOffsetEXT(layout, binding, &offset);

    const auto size = context().descriptor_size(vkb::DescriptorType::StorageImage);
    vkb::DescriptorImageInfo image_info{
        .imageView = *view,
        .imageLayout = vkb::ImageLayout::General,
    };
    vkb::DescriptorGetInfoEXT get_info{
        .sType = vkb::StructureType::DescriptorGetInfoEXT,
        .type = vkb::DescriptorType::StorageImage,
        .data{
            .pStorageImage = &image_info,
        },
    };
    context().vkGetDescriptorEXT(&get_info, size, mapped<uint8_t>() + offset + element * size);
}

Context &Buffer::context() const {
    return m_allocation.allocator()->context();
}

vkb::DeviceAddress Buffer::device_address() const {
    VULL_ASSERT(m_device_address != 0);
    return m_device_address;
}

} // namespace vull::vk
