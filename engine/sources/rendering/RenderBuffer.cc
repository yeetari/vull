#include <vull/rendering/RenderBuffer.hh>

#include <vull/rendering/ExecutableGraph.hh>
#include <vull/vulkan/Device.hh>

namespace {

VkBufferUsageFlags buffer_usage(BufferType type) {
    switch (type) {
    case BufferType::IndexBuffer:
        return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    case BufferType::StorageBuffer:
        return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    case BufferType::UniformBuffer:
        return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    case BufferType::VertexBuffer:
        return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    default:
        ENSURE_NOT_REACHED();
    }
}

VkDescriptorType descriptor_type(BufferType type) {
    switch (type) {
    case BufferType::StorageBuffer:
        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case BufferType::UniformBuffer:
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    default:
        ENSURE_NOT_REACHED();
    }
}

} // namespace

RenderBuffer::RenderBuffer(BufferType type, MemoryUsage usage) : MemoryResource(usage), m_type(type) {
    switch (type) {
    case BufferType::IndexBuffer:
        set_name("index buffer");
        break;
    case BufferType::StorageBuffer:
        set_name("storage buffer");
        break;
    case BufferType::UniformBuffer:
        set_name("uniform buffer");
        break;
    case BufferType::VertexBuffer:
        set_name("vertex buffer");
        break;
    }
}

RenderBuffer::~RenderBuffer() {
    vkDestroyBuffer(**m_device, m_buffer, nullptr);
    vkFreeMemory(**m_device, m_memory, nullptr);
}

void RenderBuffer::ensure_size(VkDeviceSize size) {
    if (m_size >= size) {
        return;
    }
    m_size = size;

    // Destroy old buffer.
    auto *device = **m_device;
    vkDestroyBuffer(device, m_buffer, nullptr);
    vkFreeMemory(device, m_memory, nullptr);

    // Create new buffer with required size.
    VkBufferUsageFlags usage = buffer_usage(m_type);
    if (m_usage == MemoryUsage::Transfer) {
        usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }
    VkBufferCreateInfo buffer_ci{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    ENSURE(vkCreateBuffer(device, &buffer_ci, nullptr, &m_buffer) == VK_SUCCESS);

    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(device, m_buffer, &memory_requirements);
    m_memory = m_device->allocate_memory(
        memory_requirements, m_usage == MemoryUsage::HostVisible ? MemoryType::CpuToGpu : MemoryType::GpuOnly, false,
        m_buffer, nullptr);
    ENSURE(vkBindBufferMemory(device, m_buffer, m_memory, 0) == VK_SUCCESS);

    // Index and vertex buffers never have associated descriptors.
    if (m_type == BufferType::IndexBuffer || m_type == BufferType::VertexBuffer) {
        return;
    }

    // Update descriptor.
    VkDescriptorBufferInfo buffer_info{
        .buffer = m_buffer,
        .range = VK_WHOLE_SIZE,
    };
    auto update_descriptor = [this, &buffer_info](const RenderStage *stage) {
        ENSURE_NOT_REACHED();
        VkWriteDescriptorSet write{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = nullptr,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = descriptor_type(m_type),
            .pBufferInfo = &buffer_info,
        };
        vkUpdateDescriptorSets(**m_device, 1, &write, 0, nullptr);
    };
    for (const auto *reader : m_readers) {
        update_descriptor(reader);
    }
    for (const auto *writer : m_writers) {
        update_descriptor(writer);
    }
}

void RenderBuffer::transfer(const void *data, VkDeviceSize size) {
    ASSERT(m_executable_graph != nullptr);
    ASSERT(m_usage == MemoryUsage::Transfer);
    ensure_size(size);

    auto staging_buffer = m_executable_graph->create_staging_buffer(data, size);
    m_executable_graph->enqueue_buffer_transfer(BufferTransfer{
        .src = staging_buffer,
        .dst = m_buffer,
        .size = size,
    });
}

void RenderBuffer::upload(const void *data, VkDeviceSize size) {
    ASSERT(m_executable_graph != nullptr);
    ASSERT(m_usage == MemoryUsage::HostVisible);
    ensure_size(size);

    // TODO: Keep persistently mapped?
    void *mapped_data;
    vkMapMemory(**m_device, m_memory, 0, VK_WHOLE_SIZE, 0, &mapped_data);
    std::memcpy(mapped_data, data, size);
    vkUnmapMemory(**m_device, m_memory);
}
