#include <vull/platform/FileStream.hh>

#include <vull/support/Array.hh>
#include <vull/support/Result.hh>
#include <vull/support/Span.hh>
#include <vull/support/Stream.hh>
#include <vull/support/StreamError.hh>
#include <vull/support/UniquePtr.hh>

#include <unistd.h>

namespace vull {

UniquePtr<Stream> FileStream::clone_unique() const {
    return vull::make_unique<FileStream>(FileStream(m_fd));
}

Result<size_t, StreamError> FileStream::seek(StreamOffset offset, SeekMode mode) {
    switch (mode) {
    case SeekMode::Set:
        m_head = static_cast<size_t>(offset);
        break;
    case SeekMode::Add:
        // TODO: Check for overflow.
        m_head = static_cast<size_t>(static_cast<ssize_t>(m_head) + offset);
        break;
    case SeekMode::End:
        return StreamError::NotImplemented;
    }
    return m_head;
}

Result<void, StreamError> FileStream::read(Span<void> data) {
    ssize_t rc = pread(m_fd, data.data(), data.size(), static_cast<off_t>(m_head));
    if (rc < 0) {
        return StreamError::Unknown;
    }

    const auto bytes_read = static_cast<size_t>(rc);
    m_head += bytes_read;
    if (bytes_read != data.size()) {
        return StreamError::Truncated;
    }
    return {};
}

Result<void, StreamError> FileStream::write(Span<const void> data) {
    ssize_t rc = pwrite(m_fd, data.data(), data.size(), static_cast<off_t>(m_head));
    if (rc < 0) {
        return StreamError::Unknown;
    }

    const auto bytes_written = static_cast<size_t>(rc);
    m_head += bytes_written;
    if (bytes_written != data.size()) {
        return StreamError::Truncated;
    }
    return {};
}

} // namespace vull
