#pragma once

#include <vull/support/span.hh>
#include <vull/tasklet/promise.hh>

#include <stdint.h>

namespace vull::platform {

class Event;

} // namespace vull::platform

namespace vull::vk {

class Fence;

} // namespace vull::vk

namespace vull::tasklet {

class Tasklet;

using IoResult = int32_t;

enum class IoRequestKind {
    Nop,
    PollEvent,
    WaitEvent,
    WaitVkFence,
};

// Inheriting from SharedPromise like this means that all IO request types must be trivially destructible.
class IoRequest : public SharedPromise<IoResult> {
    const IoRequestKind m_kind;

protected:
    explicit IoRequest(IoRequestKind kind) : m_kind(kind) {}

public:
    IoRequestKind kind() const { return m_kind; }
};

class NopRequest : public IoRequest {
public:
    NopRequest() : IoRequest(IoRequestKind::Nop) {}
};

class PollEventRequest : public IoRequest {
    platform::Event &m_event;
    bool m_multishot;

public:
    PollEventRequest(platform::Event &event, bool multishot)
        : IoRequest(IoRequestKind::PollEvent), m_event(event), m_multishot(multishot) {}

    platform::Event &event() const { return m_event; }
    bool multishot() const { return m_multishot; }
};

class WaitEventRequest : public IoRequest {
    platform::Event &m_event;
    uint64_t m_value;

public:
    explicit WaitEventRequest(platform::Event &event) : IoRequest(IoRequestKind::WaitEvent), m_event(event) {}

    platform::Event &event() const { return m_event; }
    uint64_t &value() { return m_value; }
};

class WaitVkFenceRequest : public IoRequest {
    const vk::Fence &m_fence;
    int m_fd;

public:
    explicit WaitVkFenceRequest(const vk::Fence &fence) : IoRequest(IoRequestKind::WaitVkFence), m_fence(fence) {}

    void set_fd(int fd) { m_fd = fd; }

    const vk::Fence &fence() const { return m_fence; }
    int fd() const { return m_fd; }
};

} // namespace vull::tasklet
