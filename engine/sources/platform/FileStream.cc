#include <vull/platform/FileStream.hh>

#include <vull/support/Stream.hh> // IWYU pragma: keep
#include <vull/support/UniquePtr.hh>

namespace vull {

UniquePtr<Stream> FileStream::clone_unique() const {
    return vull::make_unique<FileStream>(FileStream(m_fd, m_seekable));
}

} // namespace vull
