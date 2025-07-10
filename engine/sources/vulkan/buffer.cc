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

Context &Buffer::context() const {
    return m_allocation.allocator()->context();
}

vkb::DeviceAddress Buffer::device_address() const {
    VULL_ASSERT(m_device_address != 0);
    return m_device_address;
}

} // namespace vull::vk
