#pragma once

#include <vull/container/vector.hh>
#include <vull/platform/file_stream.hh>
#include <vull/support/enum.hh>
#include <vull/support/result.hh>
#include <vull/support/string.hh>
#include <vull/support/string_view.hh>
#include <vull/support/utility.hh>

#include <stdint.h>

namespace vull {

enum class FileError {
    BadAccess,
    NonExistent,
    Unknown,
};

enum class OpenError {
    NonExistent,
    Unknown,
};

enum class OpenMode {
    None = 0,
    Read = 1u << 0u,
    Write = 1u << 1u,
    Create = 1u << 2u,
    Truncate = 1u << 3u,
};
inline constexpr OpenMode operator&(OpenMode lhs, OpenMode rhs) {
    return static_cast<OpenMode>(vull::to_underlying(lhs) & vull::to_underlying(rhs));
}
inline constexpr OpenMode operator|(OpenMode lhs, OpenMode rhs) {
    return static_cast<OpenMode>(vull::to_underlying(lhs) | vull::to_underlying(rhs));
}

class File {
    int m_fd;

    explicit File(int fd) : m_fd(fd) {}

public:
    static File from_fd(int fd) { return File(fd); }
    File(const File &) = delete;
    File(File &&other) : m_fd(vull::exchange(other.m_fd, -1)) {}
    ~File();

    File &operator=(const File &) = delete;
    File &operator=(File &&) = delete;

    FileStream create_stream() const;
    int fd() const { return m_fd; }
};

String dir_path(String path);
Result<File, OpenError> open_file(String path, OpenMode mode);
Result<void, FileError> read_entire_file(String path, Vector<uint8_t> &bytes);
Result<String, FileError> read_entire_file_ascii(String path);

inline StringView file_error_string(FileError error) {
    switch (error) {
    case FileError::BadAccess:
        return "bad access";
    case FileError::NonExistent:
        return "no such file or directory";
    default:
        return "unknown error";
    }
}

} // namespace vull
