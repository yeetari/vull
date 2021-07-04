#include <vull/rendering/RenderVertexBuffer.hh>

void RenderVertexBuffer::add_attribute(VkFormat format, std::uint32_t offset) {
    m_vertex_attributes.push(VkVertexInputAttributeDescription{
        .location = m_vertex_attributes.size(),
        .format = format,
        .offset = offset,
    });
}
