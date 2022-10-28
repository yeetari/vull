#pragma once

#include <vull/vulkan/Fence.hh>
#include <vull/vulkan/Semaphore.hh>

namespace vull::vk {

class Swapchain;

} // namespace vull::vk

namespace vull {

class Frame {
    vk::Fence m_fence;
    vk::Semaphore m_acquire_semaphore;
    vk::Semaphore m_present_semaphore;

public:
    explicit Frame(const vk::Context &context)
        : m_fence(context, true), m_acquire_semaphore(context), m_present_semaphore(context) {}

    const vk::Fence &fence() const { return m_fence; }
    const vk::Semaphore &acquire_semaphore() const { return m_acquire_semaphore; }
    const vk::Semaphore &present_semaphore() const { return m_present_semaphore; }
};

} // namespace vull
