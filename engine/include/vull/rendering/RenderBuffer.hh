#pragma once

#include <vull/rendering/MemoryResource.hh>
#include <vull/rendering/RenderResource.hh>

enum class BufferType {
    IndexBuffer,
    StorageBuffer,
    UniformBuffer,
    VertexBuffer,
};

// TODO: Make some things final.

class RenderBuffer : public RenderResource, public MemoryResource {
    const BufferType m_type;
    VkBuffer m_buffer{nullptr};
    VkDeviceSize m_size{0};

    void ensure_size(VkDeviceSize size);

public:
    RenderBuffer(BufferType type, MemoryUsage usage);
    RenderBuffer(const RenderBuffer &) = delete;
    RenderBuffer(RenderBuffer &&) = delete;
    ~RenderBuffer() override;

    RenderBuffer &operator=(const RenderBuffer &) = delete;
    RenderBuffer &operator=(RenderBuffer &&) = delete;

    void transfer(const void *data, VkDeviceSize size) override;
    void upload(const void *data, VkDeviceSize size) override;

    BufferType type() const { return m_type; }
    VkBuffer buffer() const { return m_buffer; }
};
