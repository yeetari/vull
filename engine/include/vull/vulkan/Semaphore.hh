#pragma once

#include <vull/support/Utility.hh>
#include <vull/vulkan/Vulkan.hh>

namespace vull::vk {

class Context;

class Semaphore {
    const Context &m_context;
    vkb::Semaphore m_semaphore;

public:
    explicit Semaphore(const Context &context);
    Semaphore(const Semaphore &) = delete;
    Semaphore(Semaphore &&other)
        : m_context(other.m_context), m_semaphore(vull::exchange(other.m_semaphore, nullptr)) {}
    ~Semaphore();

    Semaphore &operator=(const Semaphore &) = delete;
    Semaphore &operator=(Semaphore &&) = delete;

    vkb::Semaphore operator*() const { return m_semaphore; }
};

} // namespace vull::vk
