#include <vull/platform/FileStream.hh>

#include <vull/support/Array.hh>
#include <vull/support/Result.hh>
#include <vull/support/Stream.hh>
#include <vull/support/StreamError.hh>
#include <vull/support/UniquePtr.hh>

#include <sys/types.h>

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

} // namespace vull
