#pragma once

#include <vull/rendering/RenderBuffer.hh>
#include <vull/support/Vector.hh>

#include <cstdint>

class RenderVertexBuffer : public RenderBuffer {
    const std::uint32_t m_element_size;
    Vector<VkVertexInputAttributeDescription> m_vertex_attributes;

public:
    RenderVertexBuffer(MemoryUsage usage, std::uint32_t element_size)
        : RenderBuffer(BufferType::VertexBuffer, usage), m_element_size(element_size) {}

    void add_attribute(VkFormat format, std::uint32_t offset);

    std::uint32_t element_size() const { return m_element_size; }
    const Vector<VkVertexInputAttributeDescription> &vertex_attributes() const { return m_vertex_attributes; }
};
