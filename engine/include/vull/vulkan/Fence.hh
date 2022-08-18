#pragma once

#include <vull/support/Utility.hh>
#include <vull/vulkan/Vulkan.hh>

#include <stdint.h>

namespace vull::vk {

class Context;

class Fence {
    const Context &m_context;
    vkb::Fence m_fence;

public:
    Fence(const Context &context, bool signaled);
    Fence(const Fence &) = delete;
    Fence(Fence &&other) : m_context(other.m_context), m_fence(vull::exchange(other.m_fence, nullptr)) {}
    ~Fence();

    Fence &operator=(const Fence &) = delete;
    Fence &operator=(Fence &&) = delete;

    void reset() const;
    void wait(uint64_t timeout = UINT64_MAX) const;
    vkb::Fence operator*() const { return m_fence; }
};

} // namespace vull::vk
