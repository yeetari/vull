#include <vull/renderer/Buffer.hh>

#include <vull/renderer/Device.hh>
#include <vull/support/Assert.hh>

Buffer::~Buffer() {
    ASSERT(m_device != nullptr);
    vkDestroyBuffer(**m_device, m_buffer, nullptr);
    vkFreeMemory(**m_device, m_memory, nullptr);
}

void *Buffer::map_memory() {
    void *data;
    ENSURE(vkMapMemory(**m_device, m_memory, 0, VK_WHOLE_SIZE, 0, &data) == VK_SUCCESS);
    return data;
}

void Buffer::unmap_memory() {
    vkUnmapMemory(**m_device, m_memory);
}
