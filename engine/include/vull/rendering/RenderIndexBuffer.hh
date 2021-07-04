#pragma once

#include <vull/rendering/RenderBuffer.hh>

enum class IndexType {
    UInt16,
    UInt32,
};

class RenderIndexBuffer : public RenderBuffer {
    const IndexType m_index_type;

public:
    RenderIndexBuffer(MemoryUsage usage, IndexType index_type)
        : RenderBuffer(BufferType::IndexBuffer, usage), m_index_type(index_type) {}

    IndexType index_type() const { return m_index_type; }
};
