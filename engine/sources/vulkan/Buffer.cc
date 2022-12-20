#include <vull/vulkan/Buffer.hh>

#include <vull/maths/Common.hh>
#include <vull/support/Assert.hh>
#include <vull/support/Span.hh>
#include <vull/support/Utility.hh>
#include <vull/vulkan/Allocation.hh>
#include <vull/vulkan/Allocator.hh>
#include <vull/vulkan/CommandBuffer.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/MemoryUsage.hh>
#include <vull/vulkan/Queue.hh>
#include <vull/vulkan/Vulkan.hh>

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
    auto &context = m_allocation.allocator()->context();
    return context.create_buffer(m_size, vkb::BufferUsage::TransferSrc, vk::MemoryUsage::HostOnly);
}

void Buffer::copy_from(const Buffer &src, Queue &queue, CommandPool &cmd_pool) const {
    queue.immediate_submit(cmd_pool, [&](const CommandBuffer &cmd_buf) {
        vkb::BufferCopy copy{
            .size = vull::min(src.size(), m_size),
        };
        cmd_buf.copy_buffer(src, *this, copy);
    });
}

void Buffer::upload(LargeSpan<const void> data) const {
    VULL_ASSERT(mapped_raw() != nullptr);
    memcpy(mapped_raw(), data.data(), vull::min(data.size(), m_size));
}

vkb::DeviceAddress Buffer::device_address() const {
    VULL_ASSERT(m_device_address != 0);
    return m_device_address;
}

} // namespace vull::vk
