#pragma once

#include <vull/support/result.hh>
#include <vull/support/span.hh>
#include <vull/support/stream.hh>
#include <vull/support/unique_ptr.hh>

#include <stddef.h>

namespace vull {

class File;
enum class StreamError;

class FileStream final : public Stream {
    friend File;

private:
    int m_fd;
    const bool m_seekable;
    size_t m_head{0};

    FileStream(int fd, bool seekable) : m_fd(fd), m_seekable(seekable) {}

public:
    FileStream(const FileStream &) = delete;
    FileStream(FileStream &&other)
        : m_fd(vull::exchange(other.m_fd, -1)), m_seekable(other.m_seekable), m_head(vull::exchange(other.m_head, 0u)) {
    }
    ~FileStream() override;

    FileStream &operator=(const FileStream &) = delete;
    FileStream &operator=(FileStream &&) = delete;

    UniquePtr<Stream> clone_unique() const override;
    Result<size_t, StreamError> seek(StreamOffset offset, SeekMode mode) override;
    Result<size_t, StreamError> read(Span<void> data) override;
    Result<void, StreamError> write(Span<const void> data) override;
};

} // namespace vull
