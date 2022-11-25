#pragma once

#include <vull/support/Result.hh>
#include <vull/support/Span.hh>
#include <vull/support/Stream.hh>
#include <vull/support/StreamError.hh>
#include <vull/support/UniquePtr.hh>

#include <stddef.h>

namespace vull {

class File;

class FileStream final : public Stream {
    friend File;

private:
    const int m_fd;
    const bool m_seekable;
    size_t m_head{0};

    FileStream(int fd, bool seekable) : m_fd(fd), m_seekable(seekable) {}

public:
    UniquePtr<Stream> clone_unique() const override;
    Result<size_t, StreamError> seek(StreamOffset offset, SeekMode mode) override;
    Result<size_t, StreamError> read(Span<void> data) override;
    Result<void, StreamError> write(Span<const void> data) override;
};

} // namespace vull
