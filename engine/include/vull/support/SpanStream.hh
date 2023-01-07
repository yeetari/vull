#pragma once

#include <vull/support/Span.hh>
#include <vull/support/Stream.hh>

namespace vull {

class SpanStream final : public Stream {
    const Span<const void> m_span;
    uint32_t m_head{0};

public:
    explicit SpanStream(Span<const void> span) : m_span(span) {}

    Result<size_t, StreamError> seek(StreamOffset offset, SeekMode mode) override;
    Result<size_t, StreamError> read(Span<void> data) override;
};

inline Result<size_t, StreamError> SpanStream::seek(StreamOffset offset, SeekMode mode) {
    switch (mode) {
    case SeekMode::Set:
        m_head = static_cast<uint32_t>(offset);
        break;
    case SeekMode::Add:
        m_head += static_cast<uint32_t>(offset);
        break;
    default:
        return StreamError::NotImplemented;
    }
    return static_cast<size_t>(m_head);
}

inline Result<size_t, StreamError> SpanStream::read(Span<void> data) {
    const auto to_read = vull::min(data.size(), m_span.size() - m_head);
    memcpy(data.data(), m_span.byte_offset(m_head), to_read);
    m_head += to_read;
    return static_cast<size_t>(to_read);
}

} // namespace vull
