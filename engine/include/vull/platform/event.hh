#pragma once

#include <vull/support/result.hh>
#include <vull/support/utility.hh>

namespace vull::platform {

class Event {
    int m_fd;

public:
    Event();
    Event(const Event &) = delete;
    Event(Event &&other) : m_fd(vull::exchange(other.m_fd, -1)) {}
    ~Event();

    Event &operator=(const Event &) = delete;
    Event &operator=(Event &&) = delete;

    void set() const;
    void reset() const;
    void wait() const;

    int fd() const { return m_fd; }
};

} // namespace vull::platform
