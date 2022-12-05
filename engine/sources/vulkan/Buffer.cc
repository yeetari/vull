#include <vull/vulkan/Buffer.hh>

#include <vull/support/Assert.hh>
#include <vull/support/Utility.hh>
#include <vull/vulkan/Allocation.hh>
#include <vull/vulkan/Allocator.hh>
#include <vull/vulkan/Context.hh>
#include <vull/vulkan/Vulkan.hh>

namespace vull::vk {

Buffer::Buffer(Allocation &&allocation, vkb::Buffer buffer, vkb::BufferUsage usage)
    : m_allocation(vull::move(allocation)), m_buffer(buffer), m_usage(usage) {
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
    return *this;
}

vkb::DeviceAddress Buffer::device_address() const {
    VULL_ASSERT(m_device_address != 0);
    return m_device_address;
}

} // namespace vull::vk
